#include <ppp/project/cropper.hpp>

#include <ranges>

#include <QDebug>
#include <QThread>
#include <QTimer>

#include <ppp/qt_util.hpp>

#include <ppp/project/cropper_signal_router.hpp>
#include <ppp/project/image_ops.hpp>

Cropper::Cropper(std::function<const cv::Mat*(std::string_view)> get_color_cube, const Project& project)
    : GetColorCube{ std::move(get_color_cube) }
    , Data{ project.Data }
    , Cfg{ CFG }
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

void Cropper::ClearCropWork()
{
    std::lock_guard work_lock{ PendingCropWorkMutex };
    PendingCropWork.clear();
}

void Cropper::ClearPreviewWork()
{
    std::lock_guard work_lock{ PendingPreviewWorkMutex };
    PendingPreviewWork.clear();
}

void Cropper::NewProjectOpenedDiff(const Project::ProjectData& data)
{
    std::unique_lock lock{ PropertyMutex };
    Data = data;
}

void Cropper::ImageDirChangedDiff(const fs::path& image_dir, const fs::path& crop_dir)
{
    std::unique_lock lock{ PropertyMutex };
    Data.ImageDir = image_dir;
    Data.CropDir = crop_dir;
}

void Cropper::BleedChangedDiff(Length bleed)
{
    std::unique_lock lock{ PropertyMutex };
    Data.BleedEdge = bleed;
}

void Cropper::EnableUncropChangedDiff(bool enable_uncrop)
{
    std::unique_lock lock{ PropertyMutex };
    Cfg.EnableUncrop = enable_uncrop;
}

void Cropper::ColorCubeChangedDiff(const std::string& cube_name)
{
    std::unique_lock lock{ PropertyMutex };
    Cfg.ColorCube = cube_name;
}

void Cropper::BasePreviewWidthChangedDiff(Pixel base_preview_width)
{
    std::unique_lock lock{ PropertyMutex };
    Cfg.BasePreviewWidth = base_preview_width;
}

void Cropper::MaxDPIChangedDiff(PixelDensity dpi)
{
    std::unique_lock lock{ PropertyMutex };
    Cfg.MaxDPI = dpi;
}

void Cropper::CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview)
{
    PushWork(card_name, needs_crop, needs_preview);
}

void Cropper::CardRemoved(const fs::path& card_name)
{
    RemoveWork(card_name);

    std::shared_lock lock{ PropertyMutex };
    if (CFG.EnableUncrop && fs::exists(Data.ImageDir / card_name))
    {
        CardModified(card_name);
    }
    else if (fs::exists(Data.CropDir / card_name))
    {
        fs::remove(Data.CropDir / card_name);

        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
        for (const auto& entry : fs::recursive_directory_iterator(Data.CropDir))
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

    std::shared_lock lock{ PropertyMutex };
    if (CFG.EnableUncrop && fs::exists(Data.ImageDir / old_card_name))
    {
        fs::rename(Data.ImageDir / old_card_name, Data.ImageDir / new_card_name);
    }
    else if (fs::exists(Data.CropDir / old_card_name))
    {
        fs::rename(Data.CropDir / old_card_name, Data.CropDir / new_card_name);
    }

    using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
    for (const auto& entry : fs::recursive_directory_iterator(Data.CropDir))
    {
        if (entry.is_directory() && fs::exists(entry.path() / old_card_name))
        {
            fs::rename(entry.path() / old_card_name, entry.path() / new_card_name);
        }
    }
}

void Cropper::CardModified(const fs::path& card_name)
{
    PushWork(card_name, true, true);
}

void Cropper::PauseWork()
{
    Pause.store(true, std::memory_order_relaxed);
    while (ThreadsPaused != 2)
        /* spin */;
}

void Cropper::RestartWork()
{
    Pause.store(false, std::memory_order_relaxed);
    ThreadsPaused.store(0, std::memory_order_relaxed);
}

void Cropper::PushWork(const fs::path& card_name, bool needs_crop, bool needs_preview)
{
    if (needs_crop)
    {
        std::lock_guard lock{ PendingCropWorkMutex };
        if (!std::ranges::contains(PendingCropWork, card_name))
        {
            PendingCropWork.push_back(card_name);
        }
    }
    if (needs_preview)
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
    if (Pause.load(std::memory_order_relaxed))
    {
        ThreadsPaused++;
        return;
    }

    if (Quit.load(std::memory_order_relaxed))
    {
        ThreadsDone++;
        return;
    }

    if (DoCropWork())
    {
        if (CropDone.load(std::memory_order_relaxed) == 0)
        {
            this->CropWorkStart();
            CropDone.store(UpdatesBeforeDoneTrigger, std::memory_order_relaxed);
        }
    }
    else
    {
        if (CropDone.load(std::memory_order_relaxed) != 0)
        {
            if (CropDone.fetch_sub(1, std::memory_order_relaxed) == 1)
            {
                this->CropWorkDone();
            }
        }

        // Steal work from preview thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (PreviewDone.load(std::memory_order_relaxed) != 0)
        {
            DoPreviewWork(this);
        }
    }

    CropTimer->start();
}

void Cropper::PreviewWork()
{
    if (Pause.load(std::memory_order_relaxed))
    {
        ThreadsPaused++;
        return;
    }

    if (Quit.load(std::memory_order_relaxed))
    {
        ThreadsDone++;
        return;
    }

    if (DoPreviewWork(Router))
    {
        if (PreviewDone.load(std::memory_order_relaxed) == 0)
        {
            Router->PreviewWorkStart();
            PreviewDone.store(UpdatesBeforeDoneTrigger, std::memory_order_relaxed);
        }
    }
    else
    {
        if (PreviewDone.load(std::memory_order_relaxed) != 0)
        {
            if (PreviewDone.fetch_sub(1, std::memory_order_relaxed) == 1)
            {
                this->PreviewWorkDone();
            }
        }

        // Steal work from crop thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (CropDone.load(std::memory_order_relaxed) != 0)
        {
            DoCropWork();
        }
    }

    PreviewTimer->start();
}

bool Cropper::DoCropWork()
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
            std::shared_lock lock{ PropertyMutex };
            const Length bleed_edge{ Data.BleedEdge };
            const PixelDensity max_density{ Cfg.MaxDPI };

            const auto image_size{ CardSizeWithoutBleed + 2 * bleed_edge };

            const bool uncrop{ Cfg.EnableUncrop };

            const std::string color_cube_name{ Cfg.ColorCube };

            const fs::path output_dir{ GetOutputDir(Data.CropDir, Data.BleedEdge, Cfg.ColorCube) };

            const fs::path input_file{ Data.ImageDir / card_name };
            const fs::path crop_file{ Data.CropDir / card_name };
            const fs::path output_file{ output_dir / card_name };
            lock.unlock();

            const bool do_color_correction{ color_cube_name != "None" };
            const cv::Mat* color_cube{ GetColorCube(color_cube_name) };

            {
                std::lock_guard dir_lock{ DirMutex };
                if (!fs::exists(output_dir))
                {
                    fs::create_directories(output_dir);
                }
            }

            if (uncrop && !fs::exists(input_file))
            {
                const Image image{ Image::Read(crop_file) };
                const Image uncropped_image{ UncropImage(image, card_name, nullptr) };
                uncropped_image.Write(input_file, 3, 95, CardSizeWithBleed);
            }

            // TODO: Need a force here...
            if (fs::exists(output_file))
            {
                return true;
            }

            const Image image{ Image::Read(input_file) };
            const Image cropped_image{ CropImage(image, card_name, bleed_edge, max_density, nullptr) };
            if (do_color_correction)
            {
                const Image vibrant_image{ cropped_image.ApplyColorCube(*color_cube) };
                vibrant_image.Write(output_file, 3, 95, image_size);
            }
            else
            {
                cropped_image.Write(output_file, 3, 95, image_size);
            }

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
            std::shared_lock lock{ PropertyMutex };
            const PixelSize uncropped_size{ Cfg.BasePreviewWidth, dla::math::round(Cfg.BasePreviewWidth / CardRatio) };

            // Generate Preview ...
            if (fs::exists(Data.ImageDir / card_name))
            {
                const Image image{ Image::Read(Data.ImageDir / card_name).Resize(uncropped_size) };

                ImagePreview image_preview{};
                image_preview.UncroppedImage = image;
                image_preview.CroppedImage = CropImage(image, card_name, 0_mm, 1200_dpi, nullptr);

                signaller->PreviewUpdated(card_name, image_preview);
            }
            else if (Cfg.EnableUncrop && fs::exists(Data.CropDir / card_name))
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

                const Image image{ Image::Read(Data.CropDir / card_name).Resize(cropped_size) };

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
