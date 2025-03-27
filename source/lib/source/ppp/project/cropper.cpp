#include <ppp/project/cropper.hpp>

#include <ranges>

#include <QDebug>
#include <QThread>
#include <QTimer>

#include <ppp/qt_util.hpp>

#include <ppp/project/cropper_signal_router.hpp>
#include <ppp/project/image_ops.hpp>

Cropper::Cropper(const Project& project)
    : TheProject{ project }
{
}
Cropper::~Cropper()
{
    Quit.store(true, std::memory_order_relaxed);
    while (ThreadsDone != 2)
        /* spin */;

    CropThread->quit();
    PreviewThread->quit();

    CropThread->wait();
    PreviewThread->wait();

    delete CropTimer;
    delete PreviewTimer;

    delete CropThread;
    delete PreviewThread;

    delete Router;
}

void Cropper::Start()
{
    {
        std::unique_lock lock{ PropertyMutex };
        ImageDir = TheProject.Data.ImageDir;
        CropDir = TheProject.Data.CropDir;
    }

    Router = new CropperSignalRouter;
    Router->SetWork(std::bind_front(&Cropper::PreviewWork, this));
    QObject::connect(Router, &CropperSignalRouter::CropWorkStart, this, &Cropper::CropWorkStart);
    QObject::connect(Router, &CropperSignalRouter::CropWorkDone, this, &Cropper::CropWorkDone);
    QObject::connect(Router, &CropperSignalRouter::PreviewWorkStart, this, &Cropper::PreviewWorkStart);
    QObject::connect(Router, &CropperSignalRouter::PreviewWorkDone, this, &Cropper::PreviewWorkDone);
    QObject::connect(Router, &CropperSignalRouter::PreviewUpdated, this, &Cropper::PreviewUpdated);

    CropThread = new QThread{};
    CropThread->setObjectName("Cropper Crop Work Thread");
    this->moveToThread(CropThread);

    PreviewThread = new QThread{};
    PreviewThread->setObjectName("Cropper Preview Work Thread");
    Router->moveToThread(PreviewThread);

    CropTimer = new QTimer;
    CropTimer->setInterval(5);
    CropTimer->setSingleShot(true);
    QObject::connect(CropTimer, &QTimer::timeout, this, &Cropper::CropWork);
    CropTimer->start();
    CropTimer->moveToThread(CropThread);

    PreviewTimer = new QTimer;
    PreviewTimer->setInterval(5);
    PreviewTimer->setSingleShot(true);
    QObject::connect(PreviewTimer, &QTimer::timeout, Router, &CropperSignalRouter::DoWork);
    PreviewTimer->start();
    PreviewTimer->moveToThread(PreviewThread);

    CropThread->start();
    PreviewThread->start();
}

void Cropper::CardAdded(const fs::path& card_name)
{
    PushWork(card_name);
}

void Cropper::CardRemoved(const fs::path& card_name)
{
    RemoveWork(card_name);

    if (CFG.EnableUncrop && fs::exists(ImageDir / card_name))
    {
        CardModified(card_name);
    }
    else if (fs::exists(CropDir / card_name))
    {
        fs::remove(CropDir / card_name);

        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
        for (const auto& entry : fs::recursive_directory_iterator(CropDir))
        {
            if (entry.is_directory() && fs::exists(entry.path() / card_name))
            {
                fs::remove(entry.path() / card_name);
            }
        }
    }
}

void Cropper::CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name)
{
    RemoveWork(old_card_name);

    if (CFG.EnableUncrop && fs::exists(ImageDir / old_card_name))
    {
        fs::rename(ImageDir / old_card_name, ImageDir / new_card_name);
    }
    else if (fs::exists(CropDir / old_card_name))
    {
        fs::rename(CropDir / old_card_name, CropDir / new_card_name);
    }

    using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
    for (const auto& entry : fs::recursive_directory_iterator(CropDir))
    {
        if (entry.is_directory() && fs::exists(entry.path() / old_card_name))
        {
            fs::rename(entry.path() / old_card_name, entry.path() / new_card_name);
        }
    }
}

void Cropper::CardModified(const fs::path& card_name)
{
    PushWork(card_name);
}

void Cropper::PushWork(const fs::path& card_name)
{
    {
        std::lock_guard lock{ PendingCropWorkMutex };
        if (!std::ranges::contains(PendingCropWork, card_name))
        {
            PendingCropWork.push_back(card_name);
        }
    }
    {
        std::lock_guard lock{ PendingPreviewWorkMutex };
        if (!std::ranges::contains(PendingPreviewWork, card_name))
        {
            PendingPreviewWork.push_back(card_name);
        }
    }
}

void Cropper::RemoveWork(const fs::path& card_name)
{
    {
        std::lock_guard lock{ PendingCropWorkMutex };
        auto it{ std::ranges::find(PendingCropWork, card_name) };
        if (it != PendingCropWork.end())
        {
            PendingCropWork.erase(it);
        }
    }
    {
        std::lock_guard lock{ PendingPreviewWorkMutex };
        auto it{ std::ranges::find(PendingPreviewWork, card_name) };
        if (it != PendingPreviewWork.end())
        {
            PendingPreviewWork.erase(it);
        }
    }
}

void Cropper::CropWork()
{
    if (Quit.load(std::memory_order_relaxed))
    {
        ThreadsDone++;
        return;
    }

    if (DoCropWork(this))
    {
        if (CropDone.load(std::memory_order_relaxed))
        {
            this->CropWorkStart();
            CropDone.store(false, std::memory_order_relaxed);
        }

    }
    else
    {
        if (!CropDone.load(std::memory_order_relaxed))
        {
            this->CropWorkDone();
            CropDone.store(true, std::memory_order_relaxed);
        }

        // Steal work from preview thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (!PreviewDone.load(std::memory_order_relaxed) && !DoPreviewWork(this))
        {
            QThread::yieldCurrentThread();
        }

        QThread::yieldCurrentThread();
    }

    CropTimer->start();
}

void Cropper::PreviewWork()
{
    if (Quit.load(std::memory_order_relaxed))
    {
        ThreadsDone++;
        return;
    }

    if (DoPreviewWork(Router))
    {
        if (PreviewDone.load(std::memory_order_relaxed))
        {
            Router->PreviewWorkStart();
            PreviewDone.store(false, std::memory_order_relaxed);
        }
    }
    else
    {
        if (!PreviewDone.load(std::memory_order_relaxed))
        {
            Router->PreviewWorkDone();
            PreviewDone.store(true, std::memory_order_relaxed);
        }

        // Steal work from crop thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (!CropDone.load(std::memory_order_relaxed) && !DoCropWork(Router))
        {
            QThread::yieldCurrentThread();
        }
    }

    PreviewTimer->start();
}

template<class T>
bool Cropper::DoCropWork(T* /*signaller*/)
{
    auto pop_work{
        [this]() -> std::optional<fs::path>
        {
            std::lock_guard lock{ PendingCropWorkMutex };
            if (PendingCropWork.empty())
            {
                return std::nullopt;
            }

            fs::path first_work_to_do{ std::move(PendingCropWork.front()) };
            PendingCropWork.erase(PendingCropWork.begin());
            return first_work_to_do;
        },
    };

    if (auto crop_work_to_do{ pop_work() })
    {
        fs::path card_name{ std::move(crop_work_to_do).value() };

        try
        {
            // TODO: DO THE CROPPING!!!
            return true;
        }
        catch (...)
        {
            // If user updated the file we may be reading it's still being written to,
            // Hopefully that's the only reason to end up here...
            std::lock_guard lock{ PendingCropWorkMutex };
            if (!std::ranges::contains(PendingCropWork, card_name))
            {
                PendingCropWork.push_back(std::move(card_name));
            }

            // We have failed, but we tried doing work, so we return true
            return true;
        }
    }

    return false;
}

template<class T>
bool Cropper::DoPreviewWork(T* signaller)
{

    auto pop_work{
        [this]() -> std::optional<fs::path>
        {
            std::lock_guard lock{ PendingPreviewWorkMutex };
            if (PendingPreviewWork.empty())
            {
                return std::nullopt;
            }

            fs::path first_work_to_do{ std::move(PendingPreviewWork.front()) };
            PendingPreviewWork.erase(PendingPreviewWork.begin());
            return first_work_to_do;
        },
    };

    if (auto preview_work_to_do{ pop_work() })
    {
        fs::path card_name{ std::move(preview_work_to_do).value() };

        try
        {
            const PixelSize uncropped_size{ CFG.BasePreviewWidth, dla::math::round(CFG.BasePreviewWidth / CardRatio) };

            // Generate Preview ...
            if (fs::exists(ImageDir / card_name))
            {
                const Image image{ Image::Read(ImageDir / card_name).Resize(uncropped_size) };

                ImagePreview image_preview{};
                image_preview.UncroppedImage = image;
                image_preview.CroppedImage = CropImage(image, card_name, 0_mm, 1200_dpi, nullptr);

                signaller->PreviewUpdated(card_name, image_preview);
            }
            else if (CFG.EnableUncrop && fs::exists(CropDir / card_name))
            {
                const PixelSize cropped_size{
                    [&]() -> PixelSize
                    {
                        const auto [w, h]{ uncropped_size.pod() };
                        const auto [bw, bh]{ CardSizeWithBleed.pod() };
                        const auto density{ dla::math::min(w / bw, h / bh) };
                        const auto crop{ dla::math::round(0.12_in * density) };
                        return uncropped_size - 2.0f * crop;
                    }()
                };

                const Image image{ Image::Read(CropDir / card_name).Resize(cropped_size) };

                ImagePreview image_preview{};
                image_preview.CroppedImage = image;
                image_preview.UncroppedImage = UncropImage(image, card_name, nullptr);

                signaller->PreviewUpdated(card_name, image_preview);
            }

            return true;
        }
        catch (...)
        {
            // If user updated the file we may be reading it's still being written to,
            // Hopefully that's the only reason to end up here...
            std::lock_guard lock{ PendingPreviewWorkMutex };
            if (!std::ranges::contains(PendingPreviewWork, card_name))
            {
                PendingPreviewWork.push_back(std::move(card_name));
            }

            // We have failed, but we tried doing work, so we return true
            return true;
        }
    }

    return false;
}
