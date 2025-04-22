#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <hpdf.h>

#include <dla/scalar_math.h>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/backend.hpp>
#include <ppp/pdf/util.hpp>

fs::path GeneratePdf(const Project& project, PrintFn print_fn)
{
    using CrossSegment = PdfPage::CrossSegment;

    const auto output_dir{ GetOutputDir(project.Data.CropDir, project.Data.BleedEdge, CFG.ColorCube) };

    std::array<ColorRGB32f, 2> guides_colors{
        ColorRGB32f{
            static_cast<float>(project.Data.GuidesColorA.r) / 255.0f,
            static_cast<float>(project.Data.GuidesColorA.g) / 255.0f,
            static_cast<float>(project.Data.GuidesColorA.b) / 255.0f,
        },
        ColorRGB32f{
            static_cast<float>(project.Data.GuidesColorB.r) / 255.0f,
            static_cast<float>(project.Data.GuidesColorB.g) / 255.0f,
            static_cast<float>(project.Data.GuidesColorB.b) / 255.0f,
        },
    };

    const auto card_size_with_bleed{ project.CardSize() };
    const auto page_size{ project.ComputePageSize() };

    const auto [page_width, page_height]{ page_size.pod() };
    const auto [card_width, card_height]{ card_size_with_bleed.pod() };
    const auto [columns, rows]{ project.Data.CardLayout.pod() };

    const Length start_x{ project.Data.Margins.x };
    const Length start_y{ page_height - project.Data.Margins.y };

    const auto images{ DistributeCardsToPages(project, columns, rows) };

    auto pdf{ CreatePdfDocument(CFG.Backend, project, print_fn) };

#if __cpp_lib_ranges_enumerate
    for (auto [p, page_images] : images | std::views::enumerate)
    {
#else
    for (size_t p = 0; p < images.size(); p++)
    {
        const Page& page_images{ images[p] };
#endif

        auto draw_image{
            [&](PdfPage* page, const GridImage& image, size_t x, size_t y, Length dx = 0_pts, Length dy = 0_pts, bool is_backside = false)
            {
                const auto img_path{ output_dir / image.Image };
                if (fs::exists(img_path))
                {
                    const bool oversized{ image.Oversized };
                    if (oversized && is_backside)
                    {
                        x = x - 1;
                    }

                    const auto real_x{ start_x + float(x) * card_width + dx };
                    const auto real_y{ start_y - float(y + 1) * card_height + dy };
                    const auto real_w{ oversized ? 2 * card_width : card_width };
                    const auto real_h{ card_height };

                    const auto rotation{ GetCardRotation(is_backside, oversized, image.BacksideShortEdge) };
                    page->DrawImage(img_path, real_x, real_y, real_w, real_h, rotation);
                }
            }
        };

        const auto draw_guides{
            [&](PdfPage* page, size_t x, size_t y, const GridImage& card)
            {
                auto draw_cross_at_grid{
                    [&](PdfPage* page, size_t x, size_t y, CrossSegment s, Length dx, Length dy)
                    {
                        const auto real_x{ start_x + float(x) * card_width + dx };
                        const auto real_y{ start_y - float(y) * card_height + dy };
                        page->DrawDashedCross(guides_colors, real_x, real_y, s);

                        if (project.Data.ExtendedGuides)
                        {
                            if (x == 0)
                            {
                                page->DrawDashedLine(guides_colors, real_x, real_y, 0_m, real_y);
                            }
                            if (x == columns)
                            {
                                page->DrawDashedLine(guides_colors, real_x, real_y, page_width, real_y);
                            }
                            if (y == rows)
                            {
                                page->DrawDashedLine(guides_colors, real_x, real_y, real_x, 0_m);
                            }
                            if (y == 0)
                            {
                                page->DrawDashedLine(guides_colors, real_x, real_y, real_x, page_height);
                            }
                        }
                    }
                };

                const Length bleed{ card.Oversized ? 2 * project.Data.BleedEdge : project.Data.BleedEdge };
                const Length offset{ project.Data.CornerWeight * bleed };
                if (card.Oversized)
                {
                    draw_cross_at_grid(page,
                                       x + 2,
                                       y + 0,
                                       CrossSegment::TopRight,
                                       -offset,
                                       -offset);
                    draw_cross_at_grid(page,
                                       x + 2,
                                       y + 1,
                                       CrossSegment::BottomRight,
                                       -offset,
                                       +offset);
                }
                else
                {
                    draw_cross_at_grid(page,
                                       x + 1,
                                       y + 0,
                                       CrossSegment::TopRight,
                                       -offset,
                                       -offset);
                    draw_cross_at_grid(page,
                                       x + 1,
                                       y + 1,
                                       CrossSegment::BottomRight,
                                       -offset,
                                       +offset);
                }

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
                                   +offset);
            }
        };

        const auto card_grid{ DistributeCardsToGrid(page_images, true, columns, rows) };

        {
            static constexpr const char render_fmt[]{
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
                        PPP_LOG(render_fmt, p + 1, i + 1, card->Image.get().string());
                        draw_image(front_page, card.value(), x, y);
                        i++;

                        if (project.Data.EnableGuides)
                        {
                            draw_guides(front_page, x, y, card.value());
                        }
                    }
                }
            }

            front_page->Finish();
        }

        if (project.Data.BacksideEnabled)
        {
            static constexpr const char render_fmt[]{
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
                        PPP_LOG(render_fmt, p + 1, i + 1, card->Image.get().string());

                        auto backside_card{ card.value() };
                        backside_card.Image = project.GetBacksideImage(card->Image);
                        draw_image(back_page, backside_card, columns - x - 1, y, project.Data.BacksideOffset, 0_pts, true);
                        i++;

                        if (project.Data.EnableGuides && project.Data.BacksideEnableGuides)
                        {
                            draw_guides(back_page, x, y, card.value());
                        }
                    }
                }
            }

            back_page->Finish();
        }
    }

    return pdf->Write(project.Data.FileName);
}

fs::path GenerateTestPdf(const Project& project, PrintFn print_fn)
{
    const auto page_size{ project.ComputePageSize() };
    const auto [page_width, page_height]{ page_size.pod() };

    const auto page_half{ page_size / 2 };
    const auto page_fourth{ page_size / 4 };
    const auto page_eighth{ page_size / 8 };
    const auto page_sixteenth{ page_size / 16 };

    auto pdf{ CreatePdfDocument(CFG.Backend, project, print_fn) };

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
            front_page->DrawSolidLine(ColorRGB32f{}, left_line_x, 0_mm, left_line_x, page_height - page_eighth.y);

            if (project.Data.BacksideEnabled)
            {
                const Size backside_text_top_left{ left_line_x, page_height - page_eighth.y };
                const Size backside_text_bottom_right{ page_width, page_half.y };
                front_page->DrawText("Shine a light through this page, the line on the back should align with the front. "
                                     "If not, measure the difference and paste it into the backside offset option.",
                                     { backside_text_top_left, backside_text_bottom_right });
            }

            const auto right_line_x{ page_fourth.x + 20_mm };
            front_page->DrawSolidLine(ColorRGB32f{}, right_line_x, 0_mm, right_line_x, page_half.y);

            const Size text_top_left{ right_line_x, page_fourth.y };
            const Size text_bottom_right{ page_width, 0_mm };
            front_page->DrawText("These lines should be exactly 20mm apart. If not, make sure to print at 100% scaling.",
                                 { text_top_left, text_bottom_right });
        }
    }

    if (project.Data.BacksideEnabled)
    {
        auto* back_page{ pdf->NextPage() };

        const auto backside_left_line_x{ page_width - page_fourth.x + project.Data.BacksideOffset };
        back_page->DrawSolidLine(ColorRGB32f{}, backside_left_line_x, 0_mm, backside_left_line_x, page_height);
    }

    return pdf->Write("alignment.pdf");
}
