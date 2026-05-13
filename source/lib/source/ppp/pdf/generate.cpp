#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <QRunnable>
#include <QThreadPool>

#include <fmt/chrono.h>

#include <dla/scalar_math.h>
#include <dla/vector_math.h>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/backend.hpp>
#include <ppp/pdf/util.hpp>

#include <ppp/profile/profile.hpp>

class PdfWorker : public QRunnable
{
  public:
    PdfWorker(
        std::atomic_uint32_t& work_done,
        std::function<void()> work)
        : m_WorkDone{ work_done }
        , m_Work{ std::move(work) }
    {
    }

    virtual void run() override
    {
        m_Work();
        m_WorkDone.fetch_add(1, std::memory_order::release);
    }

  private:
    std::atomic_uint32_t& m_WorkDone;
    std::function<void()> m_Work;
};

using ImageRef = std::reference_wrapper<const fs::path>;
struct ImageCacheData
{
    ImageRef m_Image;
    Size m_Size;
    Image::Rotation m_Rotation;
};

std::vector<ImageCacheData> CollectUniqueImages(const std::vector<Page>& pages,
                                                const std::vector<PageImageTransform>& transforms)
{
    TRACY_AUTO_SCOPE();

    std::vector<ImageCacheData> unique_images;

#if __cpp_lib_ranges_enumerate
    for (auto [p, page] : pages | std::views::enumerate)
    {
#else
    for (size_t p = 0; p < pages.size(); p++)
    {
        const Page& page{ pages[p] };
#endif
        const auto num_images{ page.m_Images.size() };
        for (size_t i = 0; i < num_images; ++i)
        {
            const auto& card{ page.m_Images[i] };
            const auto& transform{ transforms[i] };

            if (card.m_Image.has_value())
            {
                const auto proj{
                    [&](const ImageCacheData& img)
                    {
                        return card.m_Image.value() == img.m_Image &&
                               transform.m_Size == img.m_Size &&
                               transform.m_Rotation == img.m_Rotation;
                    }
                };
                if (!std::ranges::contains(unique_images, true, proj))
                {
                    unique_images.push_back(ImageCacheData{
                        card.m_Image.value(),
                        transform.m_Size,
                        transform.m_Rotation,
                    });
                }
            }
        }
    }

    return unique_images;
}

uint32_t QueueImageCacheWork(PdfDocument* frontside_pdf,
                             const std::vector<ImageCacheData>& frontside_images,
                             PdfDocument* backside_pdf,
                             const std::vector<ImageCacheData>& backside_images,
                             std::function<const fs::path(const fs::path&)> get_frontside_file,
                             std::function<const fs::path(const fs::path&)> get_backside_file,
                             std::atomic_uint32_t& work_signal)
{
    TRACY_AUTO_SCOPE();

    const PixelDensity max_density{ g_Cfg.m_MaxDPI };

    std::vector<std::function<void()>> image_cache_work;
    for (const auto [img, size, rot] : frontside_images)
    {
        const auto img_path{ get_frontside_file(img) };
        image_cache_work.push_back(
            [=]()
            {
                LogInfo("Caching frontside image {}...", img.get().string());
                PdfDocument::ImageCacheData image_data{
                    .m_Path{ img_path },
                    .m_Size{ size },
                    .m_Rotation = rot,
                    .m_MaxDensity{ max_density },
                };
                frontside_pdf->PreCacheImage(image_data);
            });
    };
    for (const auto [img, size, rot] : backside_images)
    {
        const auto img_path{ get_backside_file(img) };
        image_cache_work.push_back(
            [=]()
            {
                LogInfo("Caching backside image {}...", img.get().string());
                PdfDocument::ImageCacheData image_data{
                    .m_Path{ img_path },
                    .m_Size{ size },
                    .m_Rotation = rot,
                    .m_MaxDensity{ max_density },
                };
                backside_pdf->PreCacheImage(image_data);
            });
    };

    const bool threaded_image_pre_cache{ IsImageCacheThreadSafe(g_Cfg.m_Backend) };
    if (!threaded_image_pre_cache || g_Cfg.m_DeterminsticPdfOutput)
    {
        for (const auto& work : image_cache_work)
        {
            work();
            work_signal++;
        }
    }
    else
    {
        for (const auto& work : image_cache_work)
        {
            auto* worker{ new PdfWorker{ work_signal, work } };
            QThreadPool::globalInstance()->start(worker);
        }
    }

    return static_cast<uint32_t>(image_cache_work.size());
}

PdfResults GeneratePdf(const Project& project)
{
    TRACY_AUTO_SCOPE();

    AtScopeExit log_generate_time{
        [start_point = std::chrono::high_resolution_clock::now()]()
        {
            const auto end_point{ std::chrono::high_resolution_clock::now() };
            const auto duration{ end_point - start_point };
            LogInfo("PDF Generation finished in {}s...",
                    std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0f);
        }
    };

    using CrossSegment = PdfPage::CrossSegment;

    const auto output_dir{ project.GetOutputFolder() };
    const auto backside_output_dir{ project.GetBacksideOutputFolder() };
    const auto get_frontside_file{
        [&project, &output_dir](const fs::path& card_name)
        {
            if (g_Cfg.m_NoCropMode)
            {
                if (fs::exists(project.m_Data.m_UncropDir / card_name))
                {
                    return project.m_Data.m_UncropDir / card_name;
                }
                return project.GetCardImagePath(card_name);
            }
            return output_dir / card_name;
        }
    };
    const auto get_backside_file{
        [&project, &backside_output_dir](const fs::path& card_name)
        {
            if (g_Cfg.m_NoCropMode)
            {
                if (fs::exists(project.m_Data.m_UncropDir / card_name))
                {
                    return project.m_Data.m_UncropDir / card_name;
                }
                return project.GetCardImagePath(card_name);
            }
            return backside_output_dir / card_name;
        }
    };

    const auto& [darker_color, lighter_color]{
        CategorizeColors(project.m_Data.m_GuidesColorA,
                         project.m_Data.m_GuidesColorB)
    };

    ColorRGB32f guides_color_a{
        static_cast<float>(darker_color.r) / 255.0f,
        static_cast<float>(darker_color.g) / 255.0f,
        static_cast<float>(darker_color.b) / 255.0f,
    };
    ColorRGB32f guides_color_b{
        static_cast<float>(lighter_color.r) / 255.0f,
        static_cast<float>(lighter_color.g) / 255.0f,
        static_cast<float>(lighter_color.b) / 255.0f,
    };

    PdfPage::DashedLineStyle line_style{
        {
            project.m_Data.m_GuidesThickness,
            guides_color_a,
        },
        guides_color_b,
        project.m_Data.m_GuidesThickness * 2,
    };

    const auto page_size{ project.ComputePageSize() };
    const auto [page_width, page_height]{ page_size.pod() };

    const auto space_for_header{ project.ComputeMargins().m_Top };
    const auto header_size{ 12_pts };
    const auto header_top{ (space_for_header - header_size) / 2.0f };
    const bool enough_space_for_header{ space_for_header >= header_size };
    const bool do_render_header{ project.m_Data.m_RenderPageHeader && enough_space_for_header };

    const auto bleed{ project.m_Data.m_BleedEdge };
    const auto envelope_bleed{ project.m_Data.m_EnvelopeBleedEdge };
    const auto corner_guides_offset{ project.m_Data.m_GuidesOffset };

    const auto pages{ DistributeCardsToPages(project) };
    const auto transforms{ ComputeTransforms(project) };

    const auto backside_pages{
        project.m_Data.m_BacksideEnabled ? MakeBacksidePages(project, pages)
                                         : std::vector<Page>{}
    };
    const auto backside_transforms{
        project.m_Data.m_BacksideEnabled ? ComputeBacksideTransforms(project, transforms)
                                         : PageImageTransforms{}
    };

    const auto num_pages{ pages.size() };

    std::vector<PdfPage::LineData> extended_guides;
    std::vector<PdfPage::LineData> backside_extended_guides;
    if (project.m_Data.m_ExtendedGuides)
    {
        static constexpr auto c_Precision{ 0.01_pts };

        const auto generate_extended_guides{
            [page_width, page_height, bleed, envelope_bleed, corner_guides_offset](const auto& transforms)
            {
                std::vector<PdfPage::LineData> guides;
                if (transforms.empty())
                {
                    return guides;
                }

                struct ApproximatePosition
                {
                    int64_t m_Approximate;
                    Length m_Exact;
                };

                std::vector<ApproximatePosition> unique_x;
                std::vector<ApproximatePosition> unique_y;
                for (const auto& transform : transforms)
                {
                    const Position position{
                        transform.m_Card.m_Position.x,
                        page_height - transform.m_Card.m_Position.y - transform.m_Card.m_Size.y,
                    };

                    using lvec2 = dla::tvec2<int64_t>;
                    const auto top_left_corner_exact{
                        position - corner_guides_offset
                    };
                    const auto top_left_corner{
                        static_cast<lvec2>(top_left_corner_exact / c_Precision)
                    };
                    if (!std::ranges::contains(unique_x, top_left_corner.x, &ApproximatePosition::m_Approximate))
                    {
                        unique_x.push_back({ top_left_corner.x, top_left_corner_exact.x });
                    }
                    if (!std::ranges::contains(unique_y, top_left_corner.y, &ApproximatePosition::m_Approximate))
                    {
                        unique_y.push_back({ top_left_corner.y, top_left_corner_exact.y });
                    }

                    const auto bottom_right_corner_exact{
                        position + transform.m_Card.m_Size + corner_guides_offset
                    };
                    const auto bottom_right_corner{
                        static_cast<lvec2>(bottom_right_corner_exact / c_Precision)
                    };
                    if (!std::ranges::contains(unique_x, bottom_right_corner.x, &ApproximatePosition::m_Approximate))
                    {
                        unique_x.push_back({ bottom_right_corner.x, bottom_right_corner_exact.x });
                    }
                    if (!std::ranges::contains(unique_y, bottom_right_corner.y, &ApproximatePosition::m_Approximate))
                    {
                        unique_y.push_back({ bottom_right_corner.y, bottom_right_corner_exact.y });
                    }
                }

                const auto extended_offset{ bleed + envelope_bleed + 1_mm };
                const auto min_by_approximation{
                    [](const auto& vec)
                    {
                        return std::ranges::min(vec, {}, &ApproximatePosition::m_Approximate).m_Exact;
                    }
                };
                const auto max_by_approximation{
                    [](const auto& vec)
                    {
                        return std::ranges::max(vec, {}, &ApproximatePosition::m_Approximate).m_Exact;
                    }
                };
                const auto x_min{ min_by_approximation(unique_x) - extended_offset };
                const auto x_max{ max_by_approximation(unique_x) + extended_offset };
                const auto y_min{ min_by_approximation(unique_y) - extended_offset };
                const auto y_max{ max_by_approximation(unique_y) + extended_offset };

                for (const auto& [_, x] : unique_x)
                {
                    guides.push_back(PdfPage::LineData{
                        .m_From{ x, y_min },
                        .m_To{ x, 0_mm },
                    });
                    guides.push_back(PdfPage::LineData{
                        .m_From{ x, y_max },
                        .m_To{ x, page_height },
                    });
                }

                for (const auto& [_, y] : unique_y)
                {
                    guides.push_back(PdfPage::LineData{
                        .m_From{ x_min, y },
                        .m_To{ 0_mm, y },
                    });
                    guides.push_back(PdfPage::LineData{
                        .m_From{ x_max, y },
                        .m_To{ page_width, y },
                    });
                }

                return guides;
            }
        };

        extended_guides = generate_extended_guides(transforms);
        backside_extended_guides = generate_extended_guides(backside_transforms);
    }

    const bool backsides_on_same_pdf{
        project.m_Data.m_BacksideEnabled && !project.m_Data.m_SeparateBacksides
    };
    const bool backsides_on_separate_pdf{
        project.m_Data.m_BacksideEnabled && project.m_Data.m_SeparateBacksides
    };

    auto frontside_pdf{ CreatePdfDocument(g_Cfg.m_Backend, project) };

    auto unique_backside_pdf{
        backsides_on_separate_pdf
            ? CreatePdfDocument(g_Cfg.m_Backend, project)
            : nullptr
    };
    const auto& backside_pdf{
        backsides_on_separate_pdf
            ? unique_backside_pdf.get()
            : frontside_pdf.get()
    };

    const auto frontside_images{ CollectUniqueImages(pages, transforms) };
    const auto backside_images{ CollectUniqueImages(backside_pages, backside_transforms) };

    std::atomic_uint32_t cache_work_done{ 0 };
    const auto amount_of_work{
        QueueImageCacheWork(frontside_pdf.get(),
                            frontside_images,
                            backside_pdf,
                            backside_images,
                            get_frontside_file,
                            get_backside_file,
                            cache_work_done)
    };
    auto wait_for_cache_work{
        [&]()
        {
            while (cache_work_done.load(std::memory_order_acquire) < amount_of_work)
            {
                QThread::yieldCurrentThread();
            }
        }
    };

    if (backsides_on_same_pdf)
    {
        frontside_pdf->ReservePages(2 * pages.size());
    }
    else
    {
        frontside_pdf->ReservePages(pages.size());
        if (backsides_on_separate_pdf)
        {
            backside_pdf->ReservePages(pages.size());
        }
    }

    const auto frontside_pdf_name{ project.m_Data.m_FileName.string() };
    const auto backside_pdf_name{ frontside_pdf_name + "_backside" };

    const auto draw_image{
        [&](PdfPage* page,
            const PageImage& image,
            const PageImageTransform& transform,
            std::function<const fs::path(const fs::path&)> get_image_file)
        {
            if (image.m_Image.has_value())
            {
                const auto img_path{ get_image_file(image.m_Image.value()) };
                if (fs::exists(img_path))
                {
                    PdfPage::ImageData image_data{
                        .m_Path{ img_path },
                        .m_Pos{
                            transform.m_Position.x,
                            page_height - transform.m_Position.y - transform.m_Size.y, // NOLINT
                        },
                        .m_Size{ transform.m_Size },
                        .m_Rotation = transform.m_Rotation,
                        .m_ClipRect{
                            transform.m_ClipRect.and_then(
                                [&](const auto& clip_rect)
                                {
                                    return std::optional{
                                        ClipRect{
                                            .m_Position{
                                                clip_rect.m_Position.x,
                                                page_height -
                                                    clip_rect.m_Position.y -
                                                    clip_rect.m_Size.y, // NOLINT
                                            },
                                            .m_Size{ clip_rect.m_Size },
                                        },
                                    };
                                }),
                        },
                    };
                    page->DrawImage(image_data);
                }
            }
        }
    };

    const auto draw_corner_guides{
        [&](PdfPage* page, const PageImageTransform& transform)
        {
            if (project.m_Data.m_GuidesLength <= 0_mm)
            {
                return;
            }

            const auto draw_cross_at_grid{
                [&](PdfPage* page, Position pos, CrossSegment s)
                {
                    PdfPage::CrossData cross{
                        .m_Pos{ pos },
                        .m_Length{ project.m_Data.m_GuidesLength },
                        .m_Segment = project.m_Data.m_CrossGuides ? CrossSegment::FullCross : s,
                    };
                    page->DrawDashedCross(cross, line_style);
                }
            };

            const Position position{
                transform.m_Card.m_Position.x,
                page_height - transform.m_Card.m_Position.y - transform.m_Card.m_Size.y,
            };
            const auto bottom_left{ position - corner_guides_offset };
            const auto top_right{ position + transform.m_Card.m_Size + corner_guides_offset };

            draw_cross_at_grid(page,
                               Position{
                                   bottom_left.x,
                                   top_right.y,
                               },
                               CrossSegment::TopLeft);
            draw_cross_at_grid(page,
                               bottom_left,
                               CrossSegment::BottomLeft);
            draw_cross_at_grid(page,
                               Position{
                                   top_right.x,
                                   bottom_left.y,
                               },
                               CrossSegment::BottomRight);
            draw_cross_at_grid(page,
                               top_right,
                               CrossSegment::TopRight);
        }
    };

    const auto draw_front_page{
        [&](PdfPage* front_page, const Page& page, size_t page_index)
        {
            const auto num_images{ page.m_Images.size() };
            for (size_t i = 0; i < num_images; ++i)
            {
                const auto& card{ page.m_Images[i] };
                const auto& transform{ transforms[i] };

                if (card.m_Image.has_value())
                {
                    LogInfo("Rendering page {}...\nImage number {} - {}",
                            page_index + 1,
                            i + 1,
                            card.m_Image.value().get().string());
                    draw_image(front_page, card, transform, get_frontside_file);
                }
            }

            if (project.m_Data.m_EnableGuides)
            {
                if (project.m_Data.m_CornerGuides)
                {
                    for (const auto& transform : transforms)
                    {
                        draw_corner_guides(front_page, transform);
                    }
                }

                if (project.m_Data.m_ExtendedGuides)
                {
                    for (const auto& guide : extended_guides)
                    {
                        front_page->DrawSolidLine(guide, line_style);
                    }
                }
            }

            const auto page_name{
                transforms.size() == 1
                    ? fmt::format("{}/{} - {}",
                                  page_index + 1,
                                  num_pages,
                                  page.m_Images.front().m_Image.has_value()
                                      ? page.m_Images.front().m_Image.value().get().string()
                                      : "<empty>")
                    : fmt::format("{} - {}/{}",
                                  frontside_pdf_name,
                                  page_index + 1,
                                  num_pages)
            };
            front_page->SetPageName(page_name);

            if (!g_Cfg.m_DeterminsticPdfOutput && do_render_header)
            {
                const Size text_top_left{ 0_mm, page_height - header_top };
                const Size text_bottom_right{ page_width, page_height - header_top - header_size };
                front_page->DrawText({
                    .m_Text{ page_name },
                    .m_BoundingBox{ text_top_left, text_bottom_right },
                    .m_Backdrop{ { 1.0f, 1.0f, 1.0f } },
                });
            }

            front_page->Finish();
        }
    };

    const auto draw_back_page{
        [&](PdfPage* back_page, const Page& backside_page, size_t page_index)
        {
            if (project.m_Data.m_BacksideRotation != 0_deg)
            {
                back_page->RotateFutureContent(project.m_Data.m_BacksideRotation);
            }

            for (size_t i = 0; i < backside_page.m_Images.size(); ++i)
            {
                const auto& card{ backside_page.m_Images[i] };
                const auto& transform{ backside_transforms[i] };

                if (card.m_Image.has_value())
                {
                    LogInfo("Rendering backside for page {}...\nImage number {} - {}",
                            page_index + 1,
                            i + 1,
                            card.m_Image.value().get().string());
                    draw_image(back_page, card, transform, get_backside_file);
                }
            }

            if (project.m_Data.m_EnableGuides && project.m_Data.m_BacksideEnableGuides)
            {
                if (project.m_Data.m_CornerGuides)
                {
                    for (const auto& transform : backside_transforms)
                    {
                        draw_corner_guides(back_page, transform);
                    }
                }

                if (project.m_Data.m_ExtendedGuides)
                {
                    for (const auto& guide : backside_extended_guides)
                    {
                        back_page->DrawSolidLine(guide, line_style);
                    }
                }
            }

            const auto page_name{
                backside_transforms.size() == 1
                    ? fmt::format("{}/{} - {}",
                                  page_index + 1,
                                  num_pages,
                                  backside_page.m_Images.front().m_Image.has_value()
                                      ? backside_page.m_Images.front().m_Image.value().get().string()
                                      : "<empty>")
                    : fmt::format("{} - {}/{}",
                                  backside_pdf_name,
                                  page_index + 1,
                                  num_pages)
            };
            back_page->SetPageName(page_name);

            if (!g_Cfg.m_DeterminsticPdfOutput && do_render_header)
            {
                const Size text_top_left{ 0_mm, page_height - header_top };
                const Size text_bottom_right{ page_width, page_height - header_top - header_size };
                back_page->DrawText({
                    .m_Text{ page_name },
                    .m_BoundingBox{ text_top_left, text_bottom_right },
                    .m_Backdrop{ { 1.0f, 1.0f, 1.0f } },
                });
            }

            back_page->Finish();
        }
    };

    std::vector<std::function<void()>> generate_work;

#if __cpp_lib_ranges_enumerate
    for (auto [p, page] : pages | std::views::enumerate)
    {
#else
    for (size_t p = 0; p < pages.size(); p++)
    {
        const Page& page{ pages[p] };
#endif

        PdfPage* front_page{ frontside_pdf->NextPage(false) };
        generate_work.push_back([draw_front_page, front_page, &page, p]()
                                { draw_front_page(front_page, page, p); });

        if (project.m_Data.m_BacksideEnabled)
        {
            PdfPage* back_page{ backside_pdf->NextPage(true) };
            const auto& backside_page{ backside_pages[p] };
            generate_work.push_back([draw_back_page, back_page, &backside_page, p]()
                                    { draw_back_page(back_page, backside_page, p); });
        }
    }

    wait_for_cache_work();

    const bool threaded_page_write{ IsPageWriteThreadSafe(g_Cfg.m_Backend) };
    if (!threaded_page_write || g_Cfg.m_DeterminsticPdfOutput || generate_work.size() < 4)
    {
        for (const auto& work : generate_work)
        {
            work();
        }
    }
    else
    {
        std::atomic_uint32_t work_done{ 0 };
        for (const auto& work : generate_work)
        {
            auto* worker{ new PdfWorker{ work_done, work } };
            QThreadPool::globalInstance()->start(worker);
        }

        while (work_done.load(std::memory_order_acquire) < generate_work.size())
        {
            QThread::yieldCurrentThread();
        }
    }

    auto frontside_pdf_path{ frontside_pdf->Write(frontside_pdf_name, g_Cfg.m_VersionOutput) };

    const auto actual_backside_pdf_name{ frontside_pdf_path.stem().string() + "_backside" };
    auto backside_pdf_path{
        backside_pdf != frontside_pdf.get() && backside_pdf != nullptr
            ? std::optional{ backside_pdf->Write(actual_backside_pdf_name, false) }
            : std::nullopt
    };

    return {
        std::move(frontside_pdf_path),
        std::move(backside_pdf_path),
    };
}

fs::path GenerateTestPdf(const Project& project)
{
    const auto page_size{ project.ComputePageSize() };
    const auto [page_width, page_height]{ page_size.pod() };

    const auto page_half{ page_size / 2 };
    const auto page_fourth{ page_size / 4 };
    const auto page_eighth{ page_size / 8 };
    const auto page_sixteenth{ page_size / 16 };

    auto top_alignment_line_y{ 0_mm };

    auto pdf{ CreatePdfDocument(g_Cfg.m_Backend, project) };

    PdfPage::LineStyle line_style{
        .m_Thickness = 0.2_mm,
        .m_Color = ColorRGB32f{},
    };

    {
        auto* front_page{ pdf->NextPage(false) };
        AtScopeExit finish_page{ [&]()
                                 { front_page->Finish(); } };

        {
            const Position text_top_left{ 0_mm, page_height - page_sixteenth.y };
            const Position text_bottom_right{ page_width, page_height - page_eighth.y };
            const auto text_bb{ front_page->DrawText({ "This is a test page, follow instructions to verify your settings will work fine for proxies.",
                                                       { text_top_left, text_bottom_right } }) };

            const auto backside_offset{ project.m_Data.m_BacksideOffset / UnitValue(g_Cfg.m_BaseUnit) };
            const auto unit_name{ UnitShortName(g_Cfg.m_BaseUnit) };
            const auto settings{
                fmt::format("Settings:\nHorizontal Backside Offset: {:.2f} {}\nVertical Backside Offset: {:.2f} {}\nBackside Rotation: {:.2f} degrees\nFlip On: {}",
                            backside_offset.x,
                            unit_name,
                            backside_offset.y,
                            unit_name,
                            project.m_Data.m_BacksideRotation / 1_deg,
                            magic_enum::enum_name(project.m_Data.m_FlipOn)),
            };
            const Position settings_top_left{ text_bb.m_TopLeft.x, text_bb.m_BottomRight.y };
            const Position settings_bottom_right{ text_bottom_right.x, settings_top_left.y - page_eighth.y };
            front_page->DrawText({ settings,
                                   { settings_top_left, settings_bottom_right } });
        }

        {
            const auto left_line_x{ page_fourth.x };
            const PdfPage::LineData left_line{
                .m_From{ left_line_x, 0_mm },
                .m_To{ left_line_x, page_height - page_eighth.y },
            };
            front_page->DrawSolidLine(left_line, line_style);

            if (project.m_Data.m_BacksideEnabled)
            {
                const Position backside_text_top_left{ left_line_x, page_height - page_eighth.y };
                const Position backside_text_bottom_right{ page_width, page_half.y };
                const auto text_bb{ front_page->DrawText({ "Shine a light through this page, the line on the back should align with the front. "
                                                           "If not, measure the difference and paste it into the backside offset option.",
                                                           { backside_text_top_left, backside_text_bottom_right } }) };

                top_alignment_line_y = text_bb.m_TopLeft.y + 5_mm;
                const PdfPage::LineData top_line{
                    .m_From{ 0_mm, top_alignment_line_y },
                    .m_To{ page_width, top_alignment_line_y },
                };
                front_page->DrawSolidLine(top_line, line_style);
            }

            const auto right_line_x{ page_fourth.x + 20_mm };
            const PdfPage::LineData right_line{
                .m_From{ right_line_x, 0_mm },
                .m_To{ right_line_x, page_half.y },
            };
            front_page->DrawSolidLine(right_line, line_style);

            const Position text_top_left{ right_line_x, page_fourth.y };
            const Position text_bottom_right{ page_width, 0_mm };
            front_page->DrawText({ "These lines should be exactly 20mm apart. If not, make sure to print at 100% scaling.",
                                   { text_top_left, text_bottom_right } });
        }
    }

    if (project.m_Data.m_BacksideEnabled)
    {
        auto* back_page{ pdf->NextPage(true) };
        AtScopeExit finish_page{ [&]()
                                 { back_page->Finish(); } };

        if (project.m_Data.m_BacksideRotation != 0_deg)
        {
            back_page->RotateFutureContent(project.m_Data.m_BacksideRotation);
        }

        const bool flip_left{ project.m_Data.m_FlipOn == FlipPageOn::LeftEdge };

        const auto backside_left_line_x{
            (flip_left ? page_width - page_fourth.x : page_fourth.x) - project.m_Data.m_BacksideOffset.x
        };
        const PdfPage::LineData line{
            .m_From{ backside_left_line_x, 0_mm },
            .m_To{ backside_left_line_x, page_height },
        };
        back_page->DrawSolidLine(line, line_style);

        const auto backside_top_line_y{
            (flip_left ? top_alignment_line_y : page_height - top_alignment_line_y) + project.m_Data.m_BacksideOffset.y
        };
        const PdfPage::LineData top_line{
            .m_From{ 0_mm, backside_top_line_y },
            .m_To{ page_width, backside_top_line_y },
        };
        back_page->DrawSolidLine(top_line, line_style);
    }

    return pdf->Write("alignment.pdf", false);
}
