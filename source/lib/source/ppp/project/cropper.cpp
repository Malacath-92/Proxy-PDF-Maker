#include <ppp/project/cropper.hpp>

#include <ranges>

#include <QDebug>
#include <QThread>
#include <QTimer>

#include <fmt/chrono.h>

#include <ppp/qt_util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/cropper_signal_router.hpp>
#include <ppp/project/image_ops.hpp>

Cropper::Cropper(std::function<const cv::Mat*(std::string_view)> get_color_cube, const Project& project)
    : m_GetColorCube{ std::move(get_color_cube) }
    , m_ImageDB{ ImageDataBase::Read(project.Data.CropDir / ".image.db") }
    , m_Data{ project.Data }
    , m_Cfg{ g_Cfg }
    , m_LoadedPreviews{ project.Data.Previews | std::views::keys | std::ranges::to<std::vector>() }
{
}
Cropper::~Cropper()
{
    m_Quit.store(true, std::memory_order_relaxed);
    while (m_ThreadsDone != 2)
        /* spin */;

    m_CropThread->quit();
    m_PreviewThread->quit();

    m_CropThread->wait();
    m_PreviewThread->wait();

    delete m_CropTimer;
    delete m_PreviewTimer;

    delete m_CropThread;
    delete m_PreviewThread;

    delete m_Router;

    m_ImageDB.Write(m_Data.CropDir / ".image.db");
}

void Cropper::Start()
{
    m_Router = new CropperSignalRouter;
    m_Router->SetWork(std::bind_front(&Cropper::PreviewWork, this));
    QObject::connect(m_Router, &CropperSignalRouter::CropWorkStart, this, &Cropper::CropWorkStart);
    QObject::connect(m_Router, &CropperSignalRouter::CropWorkDone, this, &Cropper::CropWorkDone);
    QObject::connect(m_Router, &CropperSignalRouter::PreviewWorkStart, this, &Cropper::PreviewWorkStart);
    QObject::connect(m_Router, &CropperSignalRouter::PreviewWorkDone, this, &Cropper::PreviewWorkDone);
    QObject::connect(m_Router, &CropperSignalRouter::CropProgress, this, &Cropper::CropProgress);
    QObject::connect(m_Router, &CropperSignalRouter::PreviewUpdated, this, &Cropper::PreviewUpdated);

    m_CropThread = new QThread{};
    m_CropThread->setObjectName("Cropper Crop Work Thread");
    this->moveToThread(m_CropThread);

    m_PreviewThread = new QThread{};
    m_PreviewThread->setObjectName("Cropper Preview Work Thread");
    m_Router->moveToThread(m_PreviewThread);

    m_CropTimer = new QTimer;
    m_CropTimer->setInterval(5);
    m_CropTimer->setSingleShot(true);
    QObject::connect(m_CropTimer, &QTimer::timeout, this, &Cropper::CropWork);
    m_CropTimer->start();
    m_CropTimer->moveToThread(m_CropThread);

    m_PreviewTimer = new QTimer;
    m_PreviewTimer->setInterval(5);
    m_PreviewTimer->setSingleShot(true);
    QObject::connect(m_PreviewTimer, &QTimer::timeout, m_Router, &CropperSignalRouter::DoWork);
    m_PreviewTimer->start();
    m_PreviewTimer->moveToThread(m_PreviewThread);

    m_CropThread->start();
    m_PreviewThread->start();
}

void Cropper::ClearCropWork()
{
    std::lock_guard work_lock{ m_PendingCropWorkMutex };
    m_PendingCropWork.clear();
}

void Cropper::ClearPreviewWork()
{
    std::lock_guard work_lock{ m_PendingPreviewWorkMutex };
    m_PendingPreviewWork.clear();
}

void Cropper::NewProjectOpenedDiff(const Project::ProjectData& data)
{
    {
        std::unique_lock lock{ m_ImageDBMutex };
        m_ImageDB.Write(m_Data.CropDir / ".image.db");
        m_ImageDB = ImageDataBase::Read(data.CropDir / ".image.db");
    }

    std::unique_lock lock{ m_PropertyMutex };
    m_Data = data;
    m_LoadedPreviews = m_Data.Previews | std::views::keys | std::ranges::to<std::vector>();
}

void Cropper::ImageDirChangedDiff(const fs::path& image_dir, const fs::path& crop_dir, const std::vector<fs::path>& loaded_previews)
{
    {
        std::unique_lock lock{ m_ImageDBMutex };
        m_ImageDB.Write(m_Data.CropDir / ".image.db");
        m_ImageDB = ImageDataBase::Read(crop_dir / ".image.db");
    }

    std::unique_lock lock{ m_PropertyMutex };
    m_Data.ImageDir = image_dir;
    m_Data.CropDir = crop_dir;
    m_LoadedPreviews = loaded_previews;
}

void Cropper::CardSizeChangedDiff(std::string card_size)
{
    std::unique_lock lock{ m_PropertyMutex };
    m_Data.CardSizeChoice = std::move(card_size);
}

void Cropper::BleedChangedDiff(Length bleed)
{
    std::unique_lock lock{ m_PropertyMutex };
    m_Data.BleedEdge = bleed;
}

void Cropper::EnableUncropChangedDiff(bool enable_uncrop)
{
    std::unique_lock lock{ m_PropertyMutex };
    m_Cfg.EnableUncrop = enable_uncrop;
}

void Cropper::ColorCubeChangedDiff(const std::string& cube_name)
{
    std::unique_lock lock{ m_PropertyMutex };
    m_Cfg.ColorCube = cube_name;
}

void Cropper::BasePreviewWidthChangedDiff(Pixel base_preview_width)
{
    std::unique_lock lock{ m_PropertyMutex };
    m_Cfg.BasePreviewWidth = base_preview_width;
}

void Cropper::MaxDPIChangedDiff(PixelDensity dpi)
{
    std::unique_lock lock{ m_PropertyMutex };
    m_Cfg.MaxDPI = dpi;
}

void Cropper::CardAdded(const fs::path& card_name, bool needs_crop, bool needs_preview)
{
    PushWork(card_name, needs_crop, needs_preview);
}

void Cropper::CardRemoved(const fs::path& card_name)
{
    RemoveWork(card_name);

    std::shared_lock lock{ m_PropertyMutex };
    if (g_Cfg.EnableUncrop && fs::exists(m_Data.ImageDir / card_name))
    {
        CardModified(card_name);
    }
    else if (fs::exists(m_Data.CropDir / card_name))
    {
        fs::remove(m_Data.CropDir / card_name);

        for (const auto& entry : fs::recursive_directory_iterator(m_Data.CropDir))
        {
            if (entry.is_directory() && fs::exists(entry.path() / card_name))
            {
                fs::remove(entry.path() / card_name);
            }
        }
    }

    std::erase(m_LoadedPreviews, card_name);
}

void Cropper::CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name)
{
    RemoveWork(old_card_name);

    std::shared_lock lock{ m_PropertyMutex };
    if (g_Cfg.EnableUncrop && fs::exists(m_Data.ImageDir / old_card_name))
    {
        fs::rename(m_Data.ImageDir / old_card_name, m_Data.ImageDir / new_card_name);
    }
    else if (fs::exists(m_Data.CropDir / old_card_name))
    {
        fs::rename(m_Data.CropDir / old_card_name, m_Data.CropDir / new_card_name);
    }

    for (const auto& entry : fs::recursive_directory_iterator(m_Data.CropDir))
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
    m_Pause.store(true, std::memory_order_relaxed);
    while (m_ThreadsPaused != 2)
        /* spin */;
}

void Cropper::RestartWork()
{
    m_Pause.store(false, std::memory_order_relaxed);
    m_ThreadsPaused.store(0, std::memory_order_relaxed);
}

void Cropper::PushWork(const fs::path& card_name, bool needs_crop, bool needs_preview)
{
    if (needs_crop)
    {
        std::lock_guard lock{ m_PendingCropWorkMutex };
        if (!std::ranges::contains(m_PendingCropWork, card_name))
        {
            m_PendingCropWork.push_back(card_name);
        }
    }
    if (needs_preview)
    {
        std::lock_guard lock{ m_PendingPreviewWorkMutex };
        if (!std::ranges::contains(m_PendingPreviewWork, card_name))
        {
            m_PendingPreviewWork.push_back(card_name);
        }
    }
}

void Cropper::RemoveWork(const fs::path& card_name)
{
    {
        std::lock_guard lock{ m_PendingCropWorkMutex };
        auto it{ std::ranges::find(m_PendingCropWork, card_name) };
        if (it != m_PendingCropWork.end())
        {
            m_PendingCropWork.erase(it);
        }
    }
    {
        std::lock_guard lock{ m_PendingPreviewWorkMutex };
        auto it{ std::ranges::find(m_PendingPreviewWork, card_name) };
        if (it != m_PendingPreviewWork.end())
        {
            m_PendingPreviewWork.erase(it);
        }
    }
}

void Cropper::CropWork()
{
    if (m_Pause.load(std::memory_order_relaxed))
    {
        m_ThreadsPaused++;
        return;
    }

    if (m_Quit.load(std::memory_order_relaxed))
    {
        m_ThreadsDone++;
        return;
    }

    if (DoCropWork(this))
    {
        if (m_CropDone.load(std::memory_order_relaxed) == 0)
        {
            m_CropWorkStartPoint = std::chrono::high_resolution_clock::now();
            this->CropWorkStart();
            m_TotalWorkDone.store(0, std::memory_order_relaxed);
            m_CropDone.store(c_UpdatesBeforeDoneTrigger, std::memory_order_relaxed);
        }
    }
    else
    {
        if (m_CropDone.load(std::memory_order_relaxed) != 0)
        {
            if (m_CropDone.fetch_sub(1, std::memory_order_relaxed) == 1)
            {
                this->CropWorkDone();

                {
                    std::shared_lock image_db_lock{ m_ImageDBMutex };
                    std::shared_lock property_lock{ m_PropertyMutex };
                    m_ImageDB.Write(m_Data.CropDir / ".image.db");
                }

                const auto crop_work_end_point{ std::chrono::high_resolution_clock::now() };
                const auto crop_work_duration{ crop_work_end_point - m_CropWorkStartPoint };
                LogInfo("Cropper finished...\nTotal Work Items: {}\nTotal Time Taken: {}",
                        m_TotalWorkDone.load(std::memory_order_relaxed),
                        std::chrono::duration_cast<std::chrono::seconds>(crop_work_duration));
            }
        }

        // Steal work from preview thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (m_PreviewDone.load(std::memory_order_relaxed) != 0)
        {
            DoPreviewWork(this);
        }
    }

    m_CropTimer->start();
}

void Cropper::PreviewWork()
{
    if (m_Pause.load(std::memory_order_relaxed))
    {
        m_ThreadsPaused++;
        return;
    }

    if (m_Quit.load(std::memory_order_relaxed))
    {
        m_ThreadsDone++;
        return;
    }

    if (DoPreviewWork(m_Router))
    {
        if (m_PreviewDone.load(std::memory_order_relaxed) == 0)
        {
            m_Router->PreviewWorkStart();
            m_PreviewDone.store(c_UpdatesBeforeDoneTrigger, std::memory_order_relaxed);
        }
    }
    else
    {
        if (m_PreviewDone.load(std::memory_order_relaxed) != 0)
        {
            if (m_PreviewDone.fetch_sub(1, std::memory_order_relaxed) == 1)
            {
                m_Router->PreviewWorkDone();

                {
                    std::shared_lock image_db_lock{ m_ImageDBMutex };
                    std::shared_lock property_lock{ m_PropertyMutex };
                    m_ImageDB.Write(m_Data.CropDir / ".image.db");
                }
            }
        }

        // Steal work from crop thread, only if that thread is already working on it
        // so we don't miss the done signal
        if (m_CropDone.load(std::memory_order_relaxed) != 0)
        {
            DoCropWork(m_Router);
        }
    }

    m_PreviewTimer->start();
}

template<class T>
bool Cropper::DoCropWork(T* signaller)
{
    struct Work
    {
        fs::path m_CardName;
        float m_Progress;
    };
    auto pop_work{
        [this]() -> std::optional<Work>
        {
            fs::path first_work_to_do;
            float work_left;
            {
                std::lock_guard lock{ m_PendingCropWorkMutex };
                if (m_PendingCropWork.empty())
                {
                    return std::nullopt;
                }

                first_work_to_do = std::move(m_PendingCropWork.front());
                m_PendingCropWork.erase(m_PendingCropWork.begin());
                work_left = static_cast<float>(m_PendingCropWork.size());
            }

            const float work_done{ static_cast<float>(m_TotalWorkDone.fetch_add(1, std::memory_order_relaxed)) };
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
            std::shared_lock lock{ m_PropertyMutex };
            const Length bleed_edge{ m_Data.BleedEdge };
            const PixelDensity max_density{ m_Cfg.MaxDPI };

            const auto full_bleed_edge{ m_Data.CardFullBleed(m_Cfg) };
            const auto card_size{ m_Data.CardSize(m_Cfg) };
            const auto card_size_with_bleed{ m_Data.CardSizeWithBleed(m_Cfg) };
            const auto card_size_with_full_bleed{ m_Data.CardSizeWithFullBleed(m_Cfg) };

            const bool enable_uncrop{ m_Cfg.EnableUncrop };
            const bool fancy_uncrop{ m_Cfg.EnableFancyUncrop };

            const std::string color_cube_name{ m_Cfg.ColorCube };

            const fs::path output_dir{ GetOutputDir(m_Data.CropDir, m_Data.BleedEdge, m_Cfg.ColorCube) };

            const fs::path input_file{ m_Data.ImageDir / card_name };
            const fs::path crop_file{ m_Data.CropDir / card_name };
            const fs::path output_file{ output_dir / card_name };
            lock.unlock();

            const bool do_color_correction{ color_cube_name != "None" };
            const cv::Mat* color_cube{ m_GetColorCube(color_cube_name) };

            ImageParameters image_params{
                .m_DPI{ max_density },
                .m_CardSize{ card_size },
                .m_FullBleedEdge{ full_bleed_edge },
            };

            std::vector<fs::path> new_ignore_notifications{};
            std::vector<fs::path> discard_ignore_notifications{};

            AtScopeExit flush_ignore_notifications{
                [&]()
                {
                    std::lock_guard ignore_lock{ m_IgnoreMutex };
                    for (fs::path& ignore_path : new_ignore_notifications)
                    {
                        if (!std::ranges::contains(m_IgnoreNotification, ignore_path))
                        {
                            m_IgnoreNotification.push_back(std::move(ignore_path));
                        }
                    }
                    for (const fs::path& ignore_path : discard_ignore_notifications)
                    {
                        std::erase(m_IgnoreNotification, ignore_path);
                    }
                }
            };

            const auto should_ignore{
                [&](const fs::path& file_path)
                {
                    std::lock_guard ignore_lock{ m_IgnoreMutex };
                    return std::ranges::contains(m_IgnoreNotification, file_path);
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
                // Only uncrop if there is no input-file or the input-file has
                // previously been written by us
                if (!fs::exists(input_file) || m_ImageDB.FindEntry(input_file))
                {
                    QByteArray crop_file_hash{
                        [&, this]()
                        {
                            std::shared_lock image_db_lock{ m_ImageDBMutex };
                            return m_ImageDB.TestEntry(input_file, crop_file, image_params);
                        }()
                    };

                    // non-empty hash indicates that the source has changed
                    if (!crop_file_hash.isEmpty())
                    {
                        const auto handle_ignore{
                            [&, crop_file]
                            {
                                if (should_ignore(crop_file))
                                {
                                    // we just wrote to this file, ignore the change and write to DB
                                    discard_ignore_notifications.push_back(crop_file);

                                    std::unique_lock image_db_lock{ m_ImageDBMutex };
                                    m_ImageDB.PutEntry(input_file, std::move(crop_file_hash), image_params);
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
                            const Image uncropped_image{ UncropImage(image, card_name, card_size, fancy_uncrop) };
                            uncropped_image.Write(input_file, 3, 95, card_size_with_full_bleed);

                            std::unique_lock image_db_lock{ m_ImageDBMutex };
                            m_ImageDB.PutEntry(input_file, std::move(crop_file_hash), image_params);
                        }
                    }
                }
            }

            QByteArray input_file_hash{
                [&, this]()
                {
                    std::shared_lock image_db_lock{ m_ImageDBMutex };
                    return m_ImageDB.TestEntry(output_file, input_file, image_params);
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
                    std::unique_lock image_db_lock{ m_ImageDBMutex };
                    m_ImageDB.PutEntry(output_file, std::move(input_file_hash), image_params);
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
            const Image cropped_image{ CropImage(image, card_name, card_size, full_bleed_edge, bleed_edge, max_density) };
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
            std::lock_guard lock{ m_PendingCropWorkMutex };
            if (!std::ranges::contains(m_PendingCropWork, card_name))
            {
                m_PendingCropWork.push_back(std::move(card_name));
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
            std::lock_guard lock{ m_PendingPreviewWorkMutex };
            if (m_PendingPreviewWork.empty())
            {
                return std::nullopt;
            }

            fs::path first_work_to_do{ std::move(m_PendingPreviewWork.front()) };
            m_PendingPreviewWork.erase(m_PendingPreviewWork.begin());
            return first_work_to_do;
        },
    };

    if (auto preview_work_to_do{ pop_work() })
    {
        fs::path card_name{ std::move(preview_work_to_do).value() };

        try
        {
            std::shared_lock lock{ m_PropertyMutex };
            const Pixel preview_width{ m_Cfg.BasePreviewWidth };
            const PixelSize uncropped_size{ preview_width, dla::math::round(preview_width / m_Data.CardRatio(m_Cfg)) };
            const auto full_bleed_edge{ m_Data.CardFullBleed(m_Cfg) };
            const Size card_size{ m_Data.CardSize(m_Cfg) };
            const Size card_size_with_full_bleed{ m_Data.CardSizeWithFullBleed(m_Cfg) };

            const bool enable_uncrop{ m_Cfg.EnableUncrop };
            const bool fancy_uncrop{ m_Cfg.EnableFancyUncrop };

            const fs::path input_file{ m_Data.ImageDir / card_name };
            const fs::path crop_file{ m_Data.CropDir / card_name };

            const bool has_preview{ std::ranges::contains(m_LoadedPreviews, card_name) };
            lock.unlock();

            const fs::path output_file{ fs::path{ input_file }.replace_extension(".prev") };

            // Fake image parameters
            ImageParameters image_params{
                .m_Width{ preview_width },
                .m_CardSize{ card_size },
                .m_FullBleedEdge{ full_bleed_edge },
                .m_WillWriteOutput = false,
            };

            // Generate Preview ...
            if (fs::exists(input_file))
            {
                QByteArray input_file_hash{
                    [&, this]()
                    {
                        std::shared_lock image_db_lock{ m_ImageDBMutex };
                        return m_ImageDB.TestEntry(output_file, input_file, image_params);
                    }()
                };

                // empty hash indicates that the source has not changed
                if (input_file_hash.isEmpty() && has_preview)
                {
                    return true;
                }

                const Image image{ Image::Read(input_file).Resize(uncropped_size) };

                ImagePreview image_preview{};
                image_preview.m_UncroppedImage = image;
                image_preview.m_CroppedImage = CropImage(image, card_name, card_size, full_bleed_edge, 0_mm, 1200_dpi);

                {
                    std::unique_lock image_db_lock{ m_ImageDBMutex };
                    m_ImageDB.PutEntry(output_file, std::move(input_file_hash), image_params);
                }

                signaller->PreviewUpdated(card_name, image_preview);
            }
            else if (enable_uncrop && fs::exists(crop_file))
            {
                QByteArray crop_file_hash{
                    [&, this]()
                    {
                        std::shared_lock image_db_lock{ m_ImageDBMutex };
                        return m_ImageDB.TestEntry(output_file, crop_file, image_params);
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
                image_preview.m_CroppedImage = image;
                image_preview.m_UncroppedImage = UncropImage(image, card_name, card_size, fancy_uncrop);

                signaller->PreviewUpdated(card_name, image_preview);
            }

            if (!has_preview)
            {
                m_LoadedPreviews.push_back(card_name);
            }
            return true;
        }
        catch (...)
        {
            // If user updated the file we may be reading it's still being written to,
            // Hopefully that's the only reason to end up here...
            std::lock_guard lock{ m_PendingPreviewWorkMutex };
            if (!std::ranges::contains(m_PendingPreviewWork, card_name))
            {
                m_PendingPreviewWork.push_back(std::move(card_name));
            }

            // We have failed, but we tried doing work, so we return true
            return true;
        }
    }

    return false;
}
