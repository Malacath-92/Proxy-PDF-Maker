#include <ppp/project/cropper_work.hpp>

#include <QEventLoop>
#include <QThreadPool>
#include <QTimer>

#include <ppp/project/image_database.hpp>
#include <ppp/project/image_ops.hpp>

static Project::ProjectData CopyRelevant(const Project::ProjectData& data)
{
    return Project::ProjectData{
        .m_ImageDir{ data.m_ImageDir },
        .m_CropDir{ data.m_CropDir },
        .m_UncropDir{ data.m_UncropDir },
        .m_ImageCache{ data.m_ImageCache },
        .m_BleedEdge{ data.m_BleedEdge },
        .m_CardSizeChoice{ data.m_CardSizeChoice },
    };
}

static Config CopyRelevant(const Config& config)
{
    return Config{
        .m_BasePreviewWidth{ config.m_BasePreviewWidth },
        .m_MaxDPI{ config.m_MaxDPI },
        .m_ColorCube{ config.m_ColorCube },
        .m_CardSizes{ config.m_CardSizes },
    };
}

CropperWork::CropperWork(std::atomic_uint32_t& alive_cropper_work)
    : m_AliveCropperWork{ alive_cropper_work }
{
    m_AliveCropperWork.fetch_add(1, std::memory_order_release);
}

CropperWork::~CropperWork()
{
    m_AliveCropperWork.fetch_sub(1, std::memory_order_release);
}

void CropperWork::Start()
{
    QThreadPool::globalInstance()->start(this, m_Priorty);
}

void CropperWork::Cancel()
{
    State expected{ State::Waiting };
    m_State.compare_exchange_strong(expected, State::Cancelled);
}

int CropperWork::Priority() const
{
    return m_Priorty;
}

void CropperWork::Pause()
{
    State expected{ State::Waiting };
    m_State.compare_exchange_strong(expected, State::Paused);
}
void CropperWork::Unpause()
{
    State expected{ State::Paused };
    if (m_State.compare_exchange_strong(expected, State::Waiting))
    {
        OnUnpaused(QPrivateSignal{});
    }
}

bool CropperWork::EnterRun()
{
    if (m_State.load() == State::Cancelled)
    {
        Finished(Conclusion::Cancelled);
        return false;
    }

    if (m_State.load() == State::Paused)
    {
        QEventLoop loop;
        QObject::connect(this, &CropperWork::OnUnpaused, &loop, &QEventLoop::quit);
        loop.exec();
    }

    m_State = State::Running;
    return true;
}

CropperCropWork::CropperCropWork(
    std::atomic_uint32_t& alive_cropper_work,
    std::atomic_uint32_t& running_crop_work,
    fs::path card_name,
    std::function<const cv::Mat*(std::string_view)> get_color_cube,
    ImageDataBase& image_db,
    const Project::ProjectData& data)
    : CropperWork{ alive_cropper_work }
    , m_RunningCropperWork{ running_crop_work }
    , m_CardName{ std::move(card_name) }
    , m_Rotation{ data.m_Cards.at(m_CardName).m_Rotation }
    , m_GetColorCube{ get_color_cube }
    , m_ImageDB{ image_db }
    , m_Data{ CopyRelevant(data) }
    , m_Cfg{ CopyRelevant(g_Cfg) }
{
}

void CropperCropWork::run()
{
    if (!EnterRun())
    {
        return;
    }

    m_RunningCropperWork.fetch_add(1, std::memory_order_release);
    AtScopeExit decrement_work_counter{
        [this]()
        {
            m_RunningCropperWork.fetch_sub(1, std::memory_order_release);
        }
    };

    try
    {
        const Length bleed_edge{ m_Data.m_BleedEdge };
        const PixelDensity max_density{ m_Cfg.m_MaxDPI };

        const auto full_bleed_edge{ m_Data.CardFullBleed(m_Cfg) };
        const auto card_size{ m_Data.CardSize(m_Cfg) };
        const auto card_size_with_bleed{ m_Data.CardSizeWithBleed(m_Cfg) };
        const auto card_size_with_full_bleed{ m_Data.CardSizeWithFullBleed(m_Cfg) };

        const bool fancy_uncrop{ m_Cfg.m_EnableFancyUncrop };

        const std::string color_cube_name{ m_Cfg.m_ColorCube };

        const fs::path output_dir{ GetOutputDir(m_Data.m_CropDir, m_Data.m_BleedEdge, m_Cfg.m_ColorCube) };

        const fs::path input_file{ m_Data.m_ImageDir / m_CardName };
        const fs::path crop_dir{ m_Data.m_CropDir };
        const fs::path uncrop_dir{ m_Data.m_UncropDir };

        const fs::path crop_file{ crop_dir / m_CardName };
        const fs::path output_file{ output_dir / m_CardName };

        const auto card_aspect_ratio{ card_size.x / card_size.y };
        const auto card_with_full_bleed_aspect_ratio{
            card_size_with_full_bleed.x / card_size_with_full_bleed.y
        };

        const bool do_color_correction{ color_cube_name != "None" };
        const cv::Mat* color_cube{ m_GetColorCube(color_cube_name) };

        ImageParameters image_params{
            .m_DPI{ max_density },
            .m_CardSize{ card_size },
            .m_FullBleedEdge{ full_bleed_edge },
            .m_Rotation{ m_Rotation },
        };

        QByteArray input_file_hash{
            m_ImageDB.TestEntry(output_file, input_file, image_params)
        };

        // empty hash indicates that the source has not changed
        if (input_file_hash.isEmpty())
        {
            Finished(Conclusion::Skipped);
            return;
        }

        Image source_image{ Image::Read(input_file).Rotate(m_Rotation) };

        const auto image_aspect_ratio{ source_image.AspectRatio() };
        const bool image_has_bleed{
            std::abs(image_aspect_ratio - card_with_full_bleed_aspect_ratio) <
            std::abs(image_aspect_ratio - card_aspect_ratio)
        };
        const bool image_is_precropped{ !image_has_bleed };

        if (image_is_precropped)
        {
            const auto uncropped_file_path{ uncrop_dir / m_CardName };

            QByteArray uncrop_input_file_hash{
                m_ImageDB.TestEntry(uncropped_file_path, input_file, image_params)
            };

            // empty hash indicates that the source has not changed
            if (uncrop_input_file_hash.isEmpty())
            {
                // load the uncropped file
                source_image = Image::Read(uncropped_file_path);
            }
            else
            {
                // do the uncrop, write the file, and copy to source_image
                const Image uncropped_image{ UncropImage(source_image, m_CardName, card_size, fancy_uncrop) };
                uncropped_image.Write(uncropped_file_path, 3, 100, card_size_with_full_bleed);
                source_image = std::move(uncropped_image);

                m_ImageDB.PutEntry(uncropped_file_path, std::move(uncrop_input_file_hash), image_params);
            }
        }

        const Image cropped_image{ CropImage(source_image, m_CardName, card_size, full_bleed_edge, bleed_edge, max_density) };
        if (do_color_correction)
        {
            const Image vibrant_image{ cropped_image.ApplyColorCube(*color_cube) };
            vibrant_image.Write(output_file, 3, 100, card_size_with_bleed);
        }
        else
        {
            cropped_image.Write(output_file, 3, 100, card_size_with_bleed);
        }

        // If there are any early-exists we need to move this into a RAII wrapper
        m_ImageDB.PutEntry(output_file, std::move(input_file_hash), image_params);

        Finished(Conclusion::Success);
    }
    catch (...)
    {
        if (m_Retries < c_MaxRetries)
        {
            // If user updated the file we may be reading it's still being written to,
            // Hopefully that's the only reason to end up here...
            QThreadPool::globalInstance()->tryStart(this);
            ++m_Retries;
        }
        else
        {
            Finished(Conclusion::Failure);
        }
    }
}

CropperPreviewWork::CropperPreviewWork(
    std::atomic_uint32_t& alive_cropper_work,
    fs::path card_name,
    bool force,
    ImageDataBase& image_db,
    const Project::ProjectData& data)
    : CropperWork{ alive_cropper_work }
    , m_CardName{ std::move(card_name) }
    , m_Rotation{ data.m_Cards.at(m_CardName).m_Rotation }
    , m_Force{ force }
    , m_ImageDB{ image_db }
    , m_Data{ CopyRelevant(data) }
    , m_Cfg{ CopyRelevant(g_Cfg) }
{
    m_Priorty = 1;
}

void CropperPreviewWork::run()
{
    if (!EnterRun())
    {
        return;
    }

    try
    {
        const Pixel preview_width{ m_Cfg.m_BasePreviewWidth };
        const PixelSize uncropped_size{ preview_width, dla::math::round(preview_width / m_Data.CardRatio(m_Cfg)) };
        const auto full_bleed_edge{ m_Data.CardFullBleed(m_Cfg) };
        const Size card_size{ m_Data.CardSize(m_Cfg) };
        const Size card_size_with_full_bleed{ m_Data.CardSizeWithFullBleed(m_Cfg) };

        const bool fancy_uncrop{ m_Cfg.m_EnableFancyUncrop };

        const fs::path input_file{ m_Data.m_ImageDir / m_CardName };
        const fs::path crop_file{ m_Data.m_CropDir / m_CardName };

        const auto card_aspect_ratio{ card_size.x / card_size.y };
        const auto card_with_full_bleed_aspect_ratio{
            card_size_with_full_bleed.x / card_size_with_full_bleed.y
        };

        const fs::path output_file{ fs::path{ input_file }.replace_extension(".prev") };

        // Fake image parameters
        ImageParameters image_params{
            .m_Width{ preview_width },
            .m_CardSize{ card_size },
            .m_FullBleedEdge{ full_bleed_edge },
            .m_Rotation{ m_Rotation },
            .m_WillWriteOutput = false,
        };

        {
            // ... and generate Preview ...
            if (fs::exists(input_file))
            {
                QByteArray input_file_hash{
                    m_ImageDB.TestEntry(output_file, input_file, image_params)
                };

                // empty hash indicates that the source has not changed
                if (input_file_hash.isEmpty() && !m_Force)
                {
                    Finished(Conclusion::Skipped);
                    return;
                }

                const Image source_image{ Image::Read(input_file).Rotate(m_Rotation) };

                static constexpr auto c_BadAspectRatioTolerance{
                    0.01f
                };
                static constexpr auto c_BadRotationTolerance{
                    0.01f
                };

                const auto image_aspect_ratio{ source_image.AspectRatio() };
                const auto with_bleed_diff{
                    std::abs(image_aspect_ratio - card_with_full_bleed_aspect_ratio)
                };
                const auto without_bleed_diff{
                    std::abs(image_aspect_ratio - card_aspect_ratio)
                };
                const bool image_has_bleed{
                    with_bleed_diff < without_bleed_diff
                };

                const bool bad_rotation{
                    std::abs(image_aspect_ratio - 1.0f / card_aspect_ratio) < c_BadRotationTolerance ||
                    std::abs(image_aspect_ratio - 1.0f / card_with_full_bleed_aspect_ratio) < c_BadRotationTolerance
                };

                ImagePreview image_preview{};
                if (image_has_bleed)
                {
                    const Image image{ source_image.Resize(uncropped_size) };

                    image_preview.m_UncroppedImage = image;
                    image_preview.m_CroppedImage = CropImage(image, m_CardName, card_size, full_bleed_edge, 0_mm, 1200_dpi);
                    image_preview.m_BadAspectRatio = with_bleed_diff > c_BadAspectRatioTolerance;
                    image_preview.m_BadRotation = bad_rotation;
                }
                else
                {
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

                    const Image image{ source_image.Resize(cropped_size) };

                    image_preview.m_CroppedImage = image;
                    image_preview.m_UncroppedImage = UncropImage(image, m_CardName, card_size, fancy_uncrop);
                    image_preview.m_BadAspectRatio = without_bleed_diff > c_BadAspectRatioTolerance;
                    image_preview.m_BadRotation = bad_rotation;
                }

                m_ImageDB.PutEntry(output_file, std::move(input_file_hash), image_params);

                PreviewUpdated(m_CardName, image_preview, m_Rotation);
            }
        }

        Finished(Conclusion::Success);
    }
    catch (...)
    {
        if (m_Retries < c_MaxRetries)
        {
            // If user updated the file we may be reading it's still being written to,
            // Hopefully that's the only reason to end up here...
            QThreadPool::globalInstance()->tryStart(this);
            ++m_Retries;
        }
        else
        {
            Finished(Conclusion::Failure);
        }
    }
}