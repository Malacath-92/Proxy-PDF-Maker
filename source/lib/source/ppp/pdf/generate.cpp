#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <hpdf.h>

#include <dla/scalar_math.h>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/haru_backend.hpp>
#include <ppp/pdf/util.hpp>

std::optional<fs::path> GeneratePdf(const Project& project, PrintFn print_fn)
{
    using CrossSegment = PdfPage::CrossSegment;

    const auto output_dir{ GetOutputDir(project.CropDir, project.BleedEdge, CFG.ColorCube) };

    std::array<ColorRGB32f, 2> guides_colors{
        ColorRGB32f{
            static_cast<float>(project.GuidesColorA.r) / 255.0f,
            static_cast<float>(project.GuidesColorA.g) / 255.0f,
            static_cast<float>(project.GuidesColorA.b) / 255.0f,
        },
        ColorRGB32f{
            static_cast<float>(project.GuidesColorB.r) / 255.0f,
            static_cast<float>(project.GuidesColorB.g) / 255.0f,
            static_cast<float>(project.GuidesColorB.b) / 255.0f,
        },
    };

    auto page_size{ CFG.PageSizes[project.PageSize].Dimensions };
    if (project.Orientation == "Landscape")
    {
        std::swap(page_size.x, page_size.y);
    }

    const auto [page_width, page_height]{ page_size.pod() };
    const Length card_width{ CardSizeWithoutBleed.x + 2.0f * project.BleedEdge };
    const Length card_height{ CardSizeWithoutBleed.y + 2.0f * project.BleedEdge };

    const auto columns{ static_cast<uint32_t>(std::floor(page_width / card_width)) };
    const auto rows{ static_cast<uint32_t>(std::floor(page_height / card_height)) };

    const Length start_x{ (page_width - card_width * float(columns)) / 2.0f };
    const Length start_y{ (page_height + card_height * float(rows)) / 2.0f };

    const auto images{ DistributeCardsToPages(project, columns, rows) };

    PdfDocument* pdf{ new HaruPdfDocument{ print_fn } };

    for (auto [p, page_images] : images | std::views::enumerate)
    {
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

        auto draw_cross_at_grid{
            [&](PdfPage* page, size_t x, size_t y, CrossSegment s, Length dx, Length dy)
            {
                const auto real_x{ start_x + float(x) * card_width + dx };
                const auto real_y{ start_y - float(y) * card_height + dy };
                page->DrawDashedCross(guides_colors, real_x, real_y, s);

                if (project.ExtendedGuides)
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

        const auto card_grid{ DistributeCardsToGrid(page_images, true, columns, rows) };

        {
            static constexpr const char render_fmt[]{
                "Rendering page {}...\nImage number {} - {}"
            };

            PdfPage* front_page{ pdf->NextPage(page_size) };

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

                        if (!project.EnableGuides)
                        {
                            continue;
                        }

                        const Length bleed{ card->Oversized ? 2 * project.BleedEdge : project.BleedEdge };
                        const Length offset{ project.CornerWeight * bleed };
                        if (card->Oversized)
                        {
                            draw_cross_at_grid(front_page,
                                               x + 2,
                                               y + 0,
                                               CrossSegment::TopRight,
                                               -offset,
                                               -offset);
                            draw_cross_at_grid(front_page,
                                               x + 2,
                                               y + 1,
                                               CrossSegment::BottomRight,
                                               -offset,
                                               +offset);
                        }
                        else
                        {
                            draw_cross_at_grid(front_page,
                                               x + 1,
                                               y + 0,
                                               CrossSegment::TopRight,
                                               -offset,
                                               -offset);
                            draw_cross_at_grid(front_page,
                                               x + 1,
                                               y + 1,
                                               CrossSegment::BottomRight,
                                               -offset,
                                               +offset);
                        }

                        draw_cross_at_grid(front_page,
                                           x,
                                           y + 0,
                                           CrossSegment::TopLeft,
                                           +offset,
                                           -offset);
                        draw_cross_at_grid(front_page,
                                           x,
                                           y + 1,
                                           CrossSegment::BottomLeft,
                                           +offset,
                                           +offset);
                    }
                }
            }
        }

        if (project.BacksideEnabled)
        {
            static constexpr const char render_fmt[]{
                "Rendering backside for page {}...\nImage number {} - {}"
            };

            PdfPage* back_page{ pdf->NextPage(page_size) };

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
                        draw_image(back_page, backside_card, columns - x - 1, y, project.BacksideOffset, 0_pts, true);
                        i++;
                    }
                }
            }
        }
    }

    return pdf->Write(project.FileName);
}
