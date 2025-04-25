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
    , ImageDB{ ImageDataBase::Read(project.Data.CropDir / ".image.db") }
    , Data{ project.Data }
    , Cfg{ CFG }
    , LoadedPreviews{ project.Data.Previews | std::views::keys | std::ranges::to<std::vector>() }
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

    ImageDB.Write(Data.CropDir / ".image.db");
}

void Cropper::Start()
{
    Router = new CropperSignalRouter;
    Router->SetWork(std::bind_front(&Cropper::PreviewWork, this));
    QObject::connect(Router, &CropperSignalRouter::CropWorkStart, this, &Cropper::CropWorkStart);
    QObject::connect(Router, &CropperSignalRouter::CropWorkDone, this, &Cropper::CropWorkDone);
    QObject::connect(Router, &CropperSignalRouter::PreviewWorkStart, this, &Cropper::PreviewWorkStart);
    QObject::connect(Router, &CropperSignalRouter::PreviewWorkDone, this, &Cropper::PreviewWorkDone);
    QObject::connect(Router, &CropperSignalRouter::CropProgress, this, &Cropper::CropProgress);
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
    {
        std::unique_lock lock{ ImageDBMutex };
        ImageDB.Write(Data.CropDir / ".image.db");
        ImageDB = ImageDataBase::Read(data.CropDir / ".image.db");
    }

    std::unique_lock lock{ PropertyMutex };
    Data = data;
    LoadedPreviews = Data.Previews | std::views::keys | std::ranges::to<std::vector>();
}

void Cropper::ImageDirChangedDiff(const fs::path& image_dir, const fs::path& crop_dir, const std::vector<fs::path>& loaded_previews)
{
    {
        std::unique_lock lock{ ImageDBMutex };
        ImageDB.Write(Data.CropDir / ".image.db");
        ImageDB = ImageDataBase::Read(crop_dir / ".image.db");
    }

    std::unique_lock lock{ PropertyMutex };
    Data.ImageDir = image_dir;
    Data.CropDir = crop_dir;
    LoadedPreviews = loaded_previews;
}

void Cropper::CardSizeChangedDiff(std::string card_size)
{
    std::unique_lock lock{ PropertyMutex };
    Data.CardSizeChoice = std::move(card_size);
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

        for (const auto& entry : fs::recursive_directory_iterator(Data.CropDir))
        {
            if (entry.is_directory() && fs::exists(entry.path() / card_name))
            {
                fs::remove(entry.path() / card_name);
            }
        }
    }

    std::erase(LoadedPreviews, card_name);
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

    if (DoCropWork(this))
    {
        if (CropDone.load(std::memory_order_relaxed) == 0)
        {
            this->CropWorkStart();
            TotalWorkDone.store(0, std::memory_order_relaxed);
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

                {
                    std::shared_lock image_db_lock{ ImageDBMutex };
                    std::shared_lock property_lock{ PropertyMutex };
                    ImageDB.Write(Data.CropDir / ".image.db");
                }
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
                Router->PreviewWorkDone();

                {
                    std::shared_lock image_db_lock{ ImageDBMutex };
                    std::shared_lock property_lock{ PropertyMutex };
                    ImageDB.Write(Data.CropDir / ".image.db");
                }
            }
        }

        // Steal work from crop thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (CropDone.load(std::memory_order_relaxed) != 0)
        {
            DoCropWork(Router);
        }
    }

    PreviewTimer->start();
}

template<class T>
bool Cropper::DoCropWork(T* signaller)
{
    struct Work
    {
        fs::path CardName;
        float Progress;
    };
    auto pop_work{
        [this]() -> std::optional<Work>
        {
            fs::path first_work_to_do;
            float work_left;
            {
                std::lock_guard lock{ PendingCropWorkMutex };
                if (PendingCropWork.empty())
                {
                    return std::nullopt;
                }

                first_work_to_do = std::move(PendingCropWork.front());
                PendingCropWork.erase(PendingCropWork.begin());
                work_left = static_cast<float>(PendingCropWork.size());
            }

            const float work_done{ static_cast<float>(TotalWorkDone.fetch_add(1, std::memory_order_relaxed)) };
            return Work{
                std::move(first_work_to_do),
                work_done / (work_done + work_left),
            };
        },
    };

    if (auto crop_work_to_do{ pop_work() })
    {
        auto [card_name, progress]{ std::move(crop_work_to_do).value() };

        try
        {
            std::shared_lock lock{ PropertyMutex };
            const Length bleed_edge{ Data.BleedEdge };
            const PixelDensity max_density{ Cfg.MaxDPI };

            const auto full_bleed_edge{ Data.CardFullBleed(Cfg) };
            const auto card_size{ Data.CardSize(Cfg) };
            const auto card_size_with_bleed{ Data.CardSizeWithBleed(Cfg) };
            const auto card_size_with_full_bleed{ Data.CardSizeWithFullBleed(Cfg) };

            const bool enable_uncrop{ Cfg.EnableUncrop };
            const bool fancy_uncrop{ Cfg.EnableFancyUncrop };

            const std::string color_cube_name{ Cfg.ColorCube };

            const fs::path output_dir{ GetOutputDir(Data.CropDir, Data.BleedEdge, Cfg.ColorCube) };

            const fs::path input_file{ Data.ImageDir / card_name };
            const fs::path crop_file{ Data.CropDir / card_name };
            const fs::path output_file{ output_dir / card_name };
            lock.unlock();

            const bool do_color_correction{ color_cube_name != "None" };
            const cv::Mat* color_cube{ GetColorCube(color_cube_name) };

            ImageParameters image_params{
                .DPI{ max_density },
                .CardSize{ card_size },
                .FullBleedEdge{ full_bleed_edge },
            };

            std::vector<fs::path> new_ignore_notifications{};
            std::vector<fs::path> discard_ignore_notifications{};

            AtScopeExit flush_ignore_notifications{
                [&]()
                {
                    std::lock_guard ignore_lock{ IgnoreMutex };
                    for (fs::path& ignore_path : new_ignore_notifications)
                    {
                        if (!std::ranges::contains(IgnoreNotification, ignore_path))
                        {
                            IgnoreNotification.push_back(std::move(ignore_path));
                        }
                    }
                    for (const fs::path& ignore_path : discard_ignore_notifications)
                    {
                        std::erase(IgnoreNotification, ignore_path);
                    }
                }
            };

            const auto should_ignore{
                [&](const fs::path& file_path)
                {
                    std::lock_guard ignore_lock{ IgnoreMutex };
                    return std::ranges::contains(IgnoreNotification, file_path);
                }
            };

            AtScopeExit update_progress{
                [&]()
                {
                    signaller->CropProgress(progress);
                }
            };

            if (enable_uncrop && fs::exists(crop_file))
            {
                QByteArray crop_file_hash{
                    [&, this]()
                    {
                        std::shared_lock image_db_lock{ ImageDBMutex };
                        return ImageDB.TestEntry(input_file, crop_file, image_params);
                    }()
                };

                // non-empty hash indicates that the source has changed
                if (!crop_file_hash.isEmpty())
                {
                    const auto handle_ignore{
                        [&]
                        {
                            if (should_ignore(crop_file))
                            {
                                // we just wrote to this file, ignore the change and write to DB
                                discard_ignore_notifications.push_back(crop_file);

                                std::unique_lock image_db_lock{ ImageDBMutex };
                                ImageDB.PutEntry(input_file, std::move(crop_file_hash), image_params);
                                return true;
                            }
                            else
                            {
                                // we will now write to this file, we expect to get a notification on its change that we should ignore
                                new_ignore_notifications.push_back(crop_file);
                                return false;
                            }
                        }
                    };

                    if (!handle_ignore())
                    {
                        const Image image{ Image::Read(crop_file) };
                        const Image uncropped_image{ UncropImage(image, card_name, card_size, fancy_uncrop, nullptr) };
                        uncropped_image.Write(input_file, 3, 95, card_size_with_full_bleed);

                        std::unique_lock image_db_lock{ ImageDBMutex };
                        ImageDB.PutEntry(input_file, std::move(crop_file_hash), image_params);
                    }
                }
            }

            QByteArray input_file_hash{
                [&, this]()
                {
                    std::shared_lock image_db_lock{ ImageDBMutex };
                    return ImageDB.TestEntry(output_file, input_file, image_params);
                }()
            };

            // empty hash indicates that the source has not changed
            if (input_file_hash.isEmpty())
            {
                return true;
            }

            AtScopeExit write_to_db{
                [&]()
                {
                    std::unique_lock image_db_lock{ ImageDBMutex };
                    ImageDB.PutEntry(output_file, std::move(input_file_hash), image_params);
                }
            };

            if (should_ignore(input_file))
            {
                // we just wrote to this file, ignore the change and write to DB
                discard_ignore_notifications.push_back(input_file);
                return true;
            }
            else if (enable_uncrop && crop_file == output_file)
            {
                // we will now write to this file, we expect to get a notification on its change that we should ignore
                new_ignore_notifications.push_back(crop_file);
            }

            const Image image{ Image::Read(input_file) };
            const Image cropped_image{ CropImage(image, card_name, card_size, full_bleed_edge, bleed_edge, max_density, nullptr) };
            if (do_color_correction)
            {
                const Image vibrant_image{ cropped_image.ApplyColorCube(*color_cube) };
                vibrant_image.Write(output_file, 3, 95, card_size_with_bleed);
            }
            else
            {
                cropped_image.Write(output_file, 3, 95, card_size_with_bleed);
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
            const Pixel preview_width{ Cfg.BasePreviewWidth };
            const PixelSize uncropped_size{ preview_width, dla::math::round(preview_width / Data.CardRatio(Cfg)) };
            const auto full_bleed_edge{ Data.CardFullBleed(Cfg) };
            const Size card_size{ Data.CardSize(Cfg) };
            const Size card_size_with_full_bleed{ Data.CardSizeWithFullBleed(Cfg) };

            const bool enable_uncrop{ Cfg.EnableUncrop };
            const bool fancy_uncrop{ Cfg.EnableFancyUncrop };

            const fs::path input_file{ Data.ImageDir / card_name };
            const fs::path crop_file{ Data.CropDir / card_name };

            const bool has_preview{ std::ranges::contains(LoadedPreviews, card_name) };
            lock.unlock();

            const fs::path output_file{ fs::path{ input_file }.replace_extension(".prev") };

            // Fake image parameters
            ImageParameters image_params{
                .Width{ preview_width },
                .CardSize{ card_size },
                .FullBleedEdge{ full_bleed_edge },
                .WillWriteOutput = false,
            };

            // Generate Preview ...
            if (fs::exists(input_file))
            {
                QByteArray input_file_hash{
                    [&, this]()
                    {
                        std::shared_lock image_db_lock{ ImageDBMutex };
                        return ImageDB.TestEntry(output_file, input_file, image_params);
                    }()
                };

                // empty hash indicates that the source has not changed
                if (input_file_hash.isEmpty() && has_preview)
                {
                    return true;
                }

                const Image image{ Image::Read(input_file).Resize(uncropped_size) };

                ImagePreview image_preview{};
                image_preview.UncroppedImage = image;
                image_preview.CroppedImage = CropImage(image, card_name, card_size, full_bleed_edge, 0_mm, 1200_dpi, nullptr);

                {
                    std::unique_lock image_db_lock{ ImageDBMutex };
                    ImageDB.PutEntry(output_file, std::move(input_file_hash), image_params);
                }

                signaller->PreviewUpdated(card_name, image_preview);
            }
            else if (enable_uncrop && fs::exists(crop_file))
            {
                QByteArray crop_file_hash{
                    [&, this]()
                    {
                        std::shared_lock image_db_lock{ ImageDBMutex };
                        return ImageDB.TestEntry(output_file, crop_file, image_params);
                    }()
                };

                // empty hash indicates that the source has not changed
                if (crop_file_hash.isEmpty() && has_preview)
                {
                    return true;
                }

                const PixelSize cropped_size{
                    [&]() -> PixelSize
                    {
                        const auto [w, h]{ uncropped_size.pod() };
                        const auto [bw, bh]{ card_size_with_full_bleed.pod() };
                        const auto density{ dla::math::min(w / bw, h / bh) };
                        const auto crop{ dla::math::round(0.12_in * density) };
                        return uncropped_size - 2.0f * crop;
                    }()
                };

                const Image image{ Image::Read(crop_file).Resize(cropped_size) };

                ImagePreview image_preview{};
                image_preview.CroppedImage = image;
                image_preview.UncroppedImage = UncropImage(image, card_name, card_size, fancy_uncrop, nullptr);

                signaller->PreviewUpdated(card_name, image_preview);
            }

            if (!has_preview)
            {
                LoadedPreviews.push_back(card_name);
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
