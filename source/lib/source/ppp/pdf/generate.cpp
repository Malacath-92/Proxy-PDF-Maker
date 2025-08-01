#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <dla/scalar_math.h>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/backend.hpp>
#include <ppp/pdf/util.hpp>

fs::path GeneratePdf(const Project& project)
{
    using CrossSegment = PdfPage::CrossSegment;

    const auto output_dir{ GetOutputDir(project.m_Data.m_CropDir, project.m_Data.m_BleedEdge, g_Cfg.m_ColorCube) };

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
    };

    const auto card_size_with_bleed{ project.CardSizeWithBleed() };
    const auto page_size{ project.ComputePageSize() };

    const auto [page_width, page_height]{ page_size.pod() };
    const auto [card_width, card_height]{ card_size_with_bleed.pod() };
    const auto [columns, rows]{ project.m_Data.m_CardLayout.pod() };
    const auto margins{ project.ComputeMargins() };
    const auto margins_four{ project.ComputeMarginsFour() };
    const auto max_margins{ project.ComputeMaxMargins() };

    // Use four-margin structure if available, otherwise fall back to old structure
    const auto start_x{ project.m_Data.m_CustomMarginsFour.has_value() ? margins_four.m_Left : margins.x };
    const auto start_y{ page_height - (project.m_Data.m_CustomMarginsFour.has_value() ? margins_four.m_Top : margins.y) };

    const auto backside_start_x{ max_margins.x - start_x };
    const auto backside_start_y{ start_y };

    const auto bleed{ project.m_Data.m_BleedEdge };
    const auto offset{ bleed - project.m_Data.m_GuidesOffset };
    const auto spacing{ project.m_Data.m_Spacing };

    const auto images{ DistributeCardsToPages(project, columns, rows) };

    auto pdf{ CreatePdfDocument(g_Cfg.m_Backend, project) };

#if __cpp_lib_ranges_enumerate
    for (auto [p, page_images] : images | std::views::enumerate)
    {
#else
    for (size_t p = 0; p < images.size(); p++)
    {
        const Page& page_images{ images[p] };
#endif

        auto draw_image{
            [&](PdfPage* page,
                const GridImage& image,
                size_t x,
                size_t y,
                Length dx = 0_pts,
                Length dy = 0_pts,
                bool is_backside = false)
            {
                const auto img_path{ output_dir / image.m_Image };
                if (fs::exists(img_path))
                {
                    const auto orig_x{ is_backside ? backside_start_x : start_x };
                    const auto orig_y{ is_backside ? backside_start_y : start_y };
                    const auto real_x{ orig_x + x * (card_width + spacing.x) + dx };
                    const auto real_y{ orig_y - (y + 1) * card_height - y * spacing.y + dy };
                    const auto real_w{ card_width };
                    const auto real_h{ card_height };

                    const auto rotation{ GetCardRotation(is_backside, image.m_BacksideShortEdge) };
                    PdfPage::ImageData image_data{
                        .m_Path{ img_path },
                        .m_Pos{ real_x, real_y },
                        .m_Size{ real_w, real_h },
                        .m_Rotation = rotation,
                    };
                    page->DrawImage(image_data);
                }
            }
        };

        const auto draw_guides{
            [&](PdfPage* page, size_t x, size_t y)
            {
                const auto draw_cross_at_grid{
                    [&](PdfPage* page, size_t x, size_t y, CrossSegment s, Length dx, Length dy)
                    {
                        const auto real_x{ start_x + x * (card_width + spacing.x) + dx };
                        // NOLINTNEXTLINE(clang-analyzer-core.NonNullParamChecker)
                        const auto real_y{ start_y - y * (card_height + spacing.y) + dy };

                        if (project.m_Data.m_CornerGuides)
                        {
                            PdfPage::CrossData cross{
                                .m_Pos{
                                    real_x,
                                    real_y,
                                },
                                .m_Length{ project.m_Data.m_GuidesLength },
                                .m_Segment = project.m_Data.m_CrossGuides ? CrossSegment::FullCross : s,
                            };
                            page->DrawDashedCross(cross, line_style);
                        }

                        if (project.m_Data.m_ExtendedGuides)
                        {
                            if (x == 0)
                            {
                                PdfPage::LineData line{
                                    .m_From{ real_x, real_y },
                                    .m_To{ 0_m, real_y },
                                };
                                page->DrawDashedLine(line, line_style);
                            }
                            if (x == columns)
                            {
                                PdfPage::LineData line{
                                    .m_From{ real_x, real_y },
                                    .m_To{ page_width, real_y },
                                };
                                page->DrawDashedLine(line, line_style);
                            }
                            if (y == rows)
                            {
                                PdfPage::LineData line{
                                    .m_From{ real_x, real_y },
                                    .m_To{ real_x, 0_m },
                                };
                                page->DrawDashedLine(line, line_style);
                            }
                            if (y == 0)
                            {
                                PdfPage::LineData line{
                                    .m_From{ real_x, real_y },
                                    .m_To{ real_x, page_height },
                                };
                                page->DrawDashedLine(line, line_style);
                            }
                        }
                    }
                };

                draw_cross_at_grid(page,
                                   x + 1,
                                   y + 0,
                                   CrossSegment::TopRight,
                                   -offset - spacing.x, // NOLINT(clang-analyzer-core.NonNullParamChecker)
                                   -offset);
                draw_cross_at_grid(page,
                                   x + 1,
                                   y + 1,
                                   CrossSegment::BottomRight,
                                   -offset - spacing.x,
                                   +offset + spacing.y);

                draw_cross_at_grid(page,
                                   x,
                                   y + 0,
                                   CrossSegment::TopLeft,
                                   +offset,
                                   -offset);
                draw_cross_at_grid(page,
                                   x,
                                   y + 1,
                                   CrossSegment::BottomLeft,
                                   +offset,
                                   +offset + spacing.y);
            }
        };

        const auto card_grid{ DistributeCardsToGrid(page_images, GridOrientation::Default, columns, rows) };

        {
            static constexpr const char c_RenderFmt[]{
                "Rendering page {}...\nImage number {} - {}"
            };

            PdfPage* front_page{ pdf->NextPage() };

            size_t i{};
            for (size_t y = 0; y < rows; y++)
            {
                for (size_t x = 0; x < columns; x++)
                {
                    if (const auto card{ card_grid[y][x] })
                    {
                        LogInfo(c_RenderFmt, p + 1, i + 1, card->m_Image.get().string());
                        draw_image(front_page, card.value(), x, y);
                        i++;

                        if (project.m_Data.m_EnableGuides)
                        {
                            draw_guides(front_page, x, y);
                        }
                    }
                }
            }

            front_page->Finish();
        }

        if (project.m_Data.m_BacksideEnabled)
        {
            static constexpr const char c_RenderFmt[]{
                "Rendering backside for page {}...\nImage number {} - {}"
            };

            PdfPage* back_page{ pdf->NextPage() };

            size_t i{};
            for (size_t y = 0; y < rows; y++)
            {
                for (size_t x = 0; x < columns; x++)
                {
                    if (const auto card{ card_grid[y][x] })
                    {
                        LogInfo(c_RenderFmt, p + 1, i + 1, card->m_Image.get().string());

                        auto backside_card{ card.value() };
                        backside_card.m_Image = project.GetBacksideImage(card->m_Image);

                        const auto flip_x{ project.m_Data.m_FlipOn == FlipPageOn::LeftEdge };
                        const auto flip_y{ !flip_x };
                        const auto bx{ flip_x ? columns - x - 1 : x };
                        const auto by{ flip_y ? rows - y - 1 : y };

                        draw_image(back_page, backside_card, bx, by, project.m_Data.m_BacksideOffset, 0_pts, true);
                        i++;

                        if (project.m_Data.m_EnableGuides && project.m_Data.m_BacksideEnableGuides)
                        {
                            draw_guides(back_page, x, y);
                        }
                    }
                }
            }

            back_page->Finish();
        }
    }

    const auto pdf_path{ pdf->Write(project.m_Data.m_FileName) };

    return pdf_path;
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

        {
            const Size text_top_left{ 0_mm, page_height - page_sixteenth.y };
            const Size text_bottom_right{ page_width, page_height - page_eighth.y };
            front_page->DrawText("This is a test page, follow instructions to verify your settings will work fine for proxies.",
                                 { text_top_left, text_bottom_right });
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
                const Size backside_text_top_left{ left_line_x, page_height - page_eighth.y };
                const Size backside_text_bottom_right{ page_width, page_half.y };
                front_page->DrawText("Shine a light through this page, the line on the back should align with the front. "
                                     "If not, measure the difference and paste it into the backside offset option.",
                                     { backside_text_top_left, backside_text_bottom_right });
            }

            const auto right_line_x{ page_fourth.x + 20_mm };
            const PdfPage::LineData right_line{
                .m_From{ right_line_x, 0_mm },
                .m_To{ right_line_x, page_half.y },
            };
            front_page->DrawSolidLine(right_line, line_style);

            const Size text_top_left{ right_line_x, page_fourth.y };
            const Size text_bottom_right{ page_width, 0_mm };
            front_page->DrawText("These lines should be exactly 20mm apart. If not, make sure to print at 100% scaling.",
                                 { text_top_left, text_bottom_right });
        }
    }

    if (project.m_Data.m_BacksideEnabled)
    {
        auto* back_page{ pdf->NextPage() };

        const auto backside_left_line_x{ page_width - page_fourth.x + project.m_Data.m_BacksideOffset };
        const PdfPage::LineData line{
            .m_From{ backside_left_line_x, 0_mm },
            .m_To{ backside_left_line_x, page_height },
        };
        back_page->DrawSolidLine(line, line_style);
    }

    return pdf->Write("alignment.pdf");
}
