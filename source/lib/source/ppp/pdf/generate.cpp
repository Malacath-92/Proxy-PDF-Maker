#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <QRunnable>
#include <QThreadPool>

#include <fmt/chrono.h>

#include <dla/scalar_math.h>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/backend.hpp>
#include <ppp/pdf/util.hpp>

PdfResults GeneratePdf(const Project& project)
{
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

    ColorRGB32f guides_color_a{
        static_cast<float>(project.m_Data.m_GuidesColorA.r) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorA.g) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorA.b) / 255.0f,
    };
    ColorRGB32f guides_color_b{
        static_cast<float>(project.m_Data.m_GuidesColorB.r) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorB.g) / 255.0f,
        static_cast<float>(project.m_Data.m_GuidesColorB.b) / 255.0f,
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
    frontside_pdf->ReservePages(pages.size() +
                                (backsides_on_same_pdf ? pages.size() : 0));

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

    const auto frontside_pdf_name{ project.m_Data.m_FileName.string() };
    const auto backside_pdf_name{ frontside_pdf_name + "_backside" };

    const auto draw_image{
        [&](PdfPage* page,
            const PageImage& image,
            const PageImageTransform& transform)
        {
            const auto img_path{ output_dir / image.m_Image };
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
    };

    const auto draw_corner_guides{
        [&](PdfPage* page, const PageImageTransform& transform)
        {
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
            static constexpr const char c_RenderFmt[]{
                "Rendering page {}...\nImage number {} - {}"
            };

            const auto num_images{ page.m_Images.size() };
            for (size_t i = 0; i < num_images; ++i)
            {
                const auto& card{ page.m_Images[i] };
                const auto& transform{ transforms[i] };

                LogInfo(c_RenderFmt, page_index + 1, i + 1, card.m_Image.get().string());
                draw_image(front_page, card, transform);
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
                        front_page->DrawDashedLine(guide, line_style);
                    }
                }
            }

            const auto page_name{
                transforms.size() == 1
                    ? fmt::format("{}/{} - {}",
                                  page_index + 1,
                                  num_pages,
                                  page.m_Images.front().m_Image.get().string())
                    : fmt::format("{} - {}/{}",
                                  frontside_pdf_name,
                                  page_index + 1,
                                  num_pages)
            };
            front_page->SetPageName(page_name);

            if (!g_Cfg.m_DeterminsticPdfOutput && enough_space_for_header)
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
            static constexpr const char c_RenderFmt[]{
                "Rendering backside for page {}...\nImage number {} - {}"
            };

            if (project.m_Data.m_BacksideRotation != 0_deg)
            {
                back_page->RotateFutureContent(project.m_Data.m_BacksideRotation);
            }

            for (size_t i = 0; i < backside_page.m_Images.size(); ++i)
            {
                const auto& card{ backside_page.m_Images[i] };
                const auto& transform{ backside_transforms[i] };

                LogInfo(c_RenderFmt, page_index + 1, i + 1, card.m_Image.get().string());
                draw_image(back_page, card, transform);
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
                        back_page->DrawDashedLine(guide, line_style);
                    }
                }
            }

            const auto page_name{
                backside_transforms.size() == 1
                    ? fmt::format("{}/{} - {}",
                                  page_index + 1,
                                  num_pages,
                                  backside_page.m_Images.front().m_Image.get().string())
                    : fmt::format("{} - {}/{}",
                                  backside_pdf_name,
                                  page_index + 1,
                                  num_pages)
            };
            back_page->SetPageName(page_name);

            if (!g_Cfg.m_DeterminsticPdfOutput && enough_space_for_header)
            {
                const Size text_top_left{ 0_mm, page_height - header_top };
                const Size text_bottom_right{ page_width, page_height - header_top - header_size };
                back_page->DrawText({
                    .m_Text{ page_name },
                    .m_BoundingBox{ text_top_left, text_bottom_right },
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

        PdfPage* front_page{ frontside_pdf->NextPage() };
        generate_work.push_back([draw_front_page, front_page, &page, p]()
                                { draw_front_page(front_page, page, p); });

        if (project.m_Data.m_BacksideEnabled)
        {
            PdfPage* back_page{ backside_pdf->NextPage() };
            const auto& backside_page{ backside_pages[p] };
            generate_work.push_back([draw_back_page, back_page, &backside_page, p]()
                                    { draw_back_page(back_page, backside_page, p); });
        }
    }

    if (g_Cfg.m_DeterminsticPdfOutput || generate_work.size() < 4)
    {
        for (const auto& work : generate_work)
        {
            work();
        }
    }
    else
    {
        class Worker : public QRunnable
        {
          public:
            Worker(
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

        std::atomic_uint32_t work_done{ 0 };
        for (const auto& work : generate_work)
        {
            auto* worker{ new Worker{ work_done, work } };
            QThreadPool::globalInstance()->start(worker);
        }

        while (work_done.load(std::memory_order_acquire) < generate_work.size())
        {
            QThread::yieldCurrentThread();
        }
    }

    auto frontside_pdf_path{ frontside_pdf->Write(frontside_pdf_name) };
    auto backside_pdf_path{
        backside_pdf != frontside_pdf.get() && backside_pdf != nullptr
            ? std::optional{ backside_pdf->Write(backside_pdf_name) }
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

    auto pdf{ CreatePdfDocument(g_Cfg.m_Backend, project) };

    PdfPage::LineStyle line_style{
        .m_Thickness = 0.2_mm,
        .m_Color = ColorRGB32f{},
    };

    {
        auto* front_page{ pdf->NextPage() };
        AtScopeExit finish_page{ [&]()
                                 { front_page->Finish(); } };

        {
            const Position text_top_left{ 0_mm, page_height - page_sixteenth.y };
            const Position text_bottom_right{ page_width, page_height - page_eighth.y };
            front_page->DrawText({ "This is a test page, follow instructions to verify your settings will work fine for proxies.",
                                   { text_top_left, text_bottom_right } });
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
                front_page->DrawText({ "Shine a light through this page, the line on the back should align with the front. "
                                       "If not, measure the difference and paste it into the backside offset option.",
                                       { backside_text_top_left, backside_text_bottom_right } });
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
        auto* back_page{ pdf->NextPage() };
        AtScopeExit finish_page{ [&]()
                                 { back_page->Finish(); } };

        const auto backside_left_line_x{ page_width - page_fourth.x + project.m_Data.m_BacksideOffset.x };
        const PdfPage::LineData line{
            .m_From{ backside_left_line_x, 0_mm },
            .m_To{ backside_left_line_x, page_height },
        };
        back_page->DrawSolidLine(line, line_style);
    }

    return pdf->Write("alignment.pdf");
}
