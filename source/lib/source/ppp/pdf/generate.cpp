#include <ppp/pdf/generate.hpp>

#include <ranges>

#include <hpdf.h>

#include <dla/scalar_math.h>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/pdf/util.hpp>

enum class CrossSegment
{
    TopLeft,
    TopRight,
    BottomRight,
    BottomLeft,
};
constexpr std::array CrossSegmentOffsets{
    dla::vec2{ +1.0f, -1.0f },
    dla::vec2{ -1.0f, -1.0f },
    dla::vec2{ -1.0f, +1.0f },
    dla::vec2{ +1.0f, +1.0f },
};

void DrawLine(HPDF_Page page, std::array<ColorRGB32f, 2> colors, HPDF_REAL fx, HPDF_REAL fy, HPDF_REAL tx, HPDF_REAL ty)
{
    HPDF_Page_SetLineWidth(page, 1.0);

    const HPDF_REAL dash_ptn[]{ 1.0f };

    // First layer
    HPDF_Page_SetDash(page, dash_ptn, 1, 0);
    HPDF_Page_SetRGBStroke(page, colors[0].r, colors[0].g, colors[0].b);
    HPDF_Page_MoveTo(page, fx, fy);
    HPDF_Page_LineTo(page, tx, ty);
    HPDF_Page_Stroke(page);

    // Second layer with phase offset
    HPDF_Page_SetDash(page, dash_ptn, 1, 1);
    HPDF_Page_SetRGBStroke(page, colors[1].r, colors[1].g, colors[1].b);
    HPDF_Page_MoveTo(page, fx, fy);
    HPDF_Page_LineTo(page, tx, ty);
    HPDF_Page_Stroke(page);
}

// Draws black-white dashed cross segment at `(x, y)`
void DrawCross(HPDF_Page page, std::array<ColorRGB32f, 2> colors, HPDF_REAL x, HPDF_REAL y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + 6.0f * dx };
    const auto ty{ y + 6.0f * dy };

    DrawLine(page, colors, x, y, tx, y);
    DrawLine(page, colors, x, y, x, ty);
}

HPDF_REAL ToReal(Length l)
{
    return static_cast<HPDF_REAL>(l / 1_pts);
}

std::optional<fs::path> GeneratePdf(const Project& project, PrintFn print_fn)
{
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

    auto error_handler{
        [](HPDF_STATUS error_no,
           HPDF_STATUS detail_no,
           void* user_data)
        {
            PrintFn& print_fn{ *static_cast<PrintFn*>(user_data) };
            PPP_LOG("HaruPDF ERROR: error_no={}, detail_no={}\n", error_no, detail_no);
        }
    };

    const auto images{ DistributeCardsToPages(project, columns, rows) };

    HPDF_Doc pdf{ HPDF_New(error_handler, &print_fn) };
    HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);

    struct ImageCacheEntry
    {
        fs::path ImagePath;
        Image::Rotation ImageRotation;
        HPDF_Image HaruImage;
    };
    std::vector<ImageCacheEntry> image_cache;
    auto get_image{
        [&](fs::path image, Image::Rotation rotation)
        {
            const auto it{
                std::ranges::find_if(image_cache, [&](const ImageCacheEntry& entry)
                                     { return entry.ImageRotation == rotation && entry.ImagePath == image; })
            };
            if (it != image_cache.end())
            {
                return it->HaruImage;
            }

            const Image loaded_image{ Image::Read(image).Rotate(rotation) };
            const auto encoded_image{ loaded_image.Encode() };
            const auto libharu_image{
                HPDF_LoadPngImageFromMem(pdf,
                                         reinterpret_cast<const HPDF_BYTE*>(encoded_image.data()),
                                         static_cast<HPDF_UINT>(encoded_image.size())),
            };
            image_cache.push_back({
                std::move(image),
                rotation,
                libharu_image,
            });
            return libharu_image;
        }
    };

    for (auto [p, page_images] : images | std::views::enumerate)
    {

        auto draw_image{
            [&](HPDF_Page page, const GridImage& image, size_t x, size_t y, Length dx = 0_pts, Length dy = 0_pts, bool is_backside = false)
            {
                const auto img_path{ output_dir / image.Image };
                if (fs::exists(img_path))
                {
                    const bool oversized{ image.Oversized };
                    if (oversized && is_backside)
                    {
                        x = x - 1;
                    }

                    const auto real_x{ ToReal(start_x + float(x) * card_width + dx) };
                    const auto real_y{ ToReal(start_y - float(y + 1) * card_height + dy) };
                    const auto real_w{ ToReal(oversized ? 2 * card_width : card_width) };
                    const auto real_h{ ToReal(card_height) };

                    const auto rotation{ GetCardRotation(is_backside, oversized, image.BacksideShortEdge) };
                    HPDF_Page_DrawImage(page, get_image(img_path, rotation), real_x, real_y, real_w, real_h);
                }
            }
        };

        auto draw_cross_at_grid{
            [&](HPDF_Page page, size_t x, size_t y, CrossSegment s, Length dx, Length dy)
            {
                const auto real_x{ ToReal(start_x + float(x) * card_width + dx) };
                const auto real_y{ ToReal(start_y - float(y) * card_height + dy) };
                DrawCross(page, guides_colors, real_x, real_y, s);

                if (project.ExtendedGuides)
                {
                    if (x == 0)
                    {
                        DrawLine(page, guides_colors, real_x, real_y, 0, real_y);
                    }
                    if (x == columns)
                    {
                        DrawLine(page, guides_colors, real_x, real_y, ToReal(page_width), real_y);
                    }
                    if (y == rows)
                    {
                        DrawLine(page, guides_colors, real_x, real_y, real_x, 0);
                    }
                    if (y == 0)
                    {
                        DrawLine(page, guides_colors, real_x, real_y, real_x, ToReal(page_height));
                    }
                }
            }
        };

        const auto card_grid{ DistributeCardsToGrid(page_images, true, columns, rows) };

        {
            static constexpr const char render_fmt[]{
                "Rendering page {}...\nImage number {} - {}"
            };

            HPDF_Page front_page{ HPDF_AddPage(pdf) };
            HPDF_Page_SetWidth(front_page, ToReal(page_size.x));
            HPDF_Page_SetHeight(front_page, ToReal(page_size.y));

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
                        if (card->Oversized)
                        {
                            draw_cross_at_grid(front_page,
                                               x + 2,
                                               y + 0,
                                               CrossSegment::TopRight,
                                               -bleed,
                                               -bleed);
                            draw_cross_at_grid(front_page,
                                               x + 2,
                                               y + 1,
                                               CrossSegment::BottomRight,
                                               -bleed,
                                               +bleed);
                        }
                        else
                        {
                            draw_cross_at_grid(front_page,
                                               x + 1,
                                               y + 0,
                                               CrossSegment::TopRight,
                                               -bleed,
                                               -bleed);
                            draw_cross_at_grid(front_page,
                                               x + 1,
                                               y + 1,
                                               CrossSegment::BottomRight,
                                               -bleed,
                                               +bleed);
                        }

                        draw_cross_at_grid(front_page,
                                           x,
                                           y + 0,
                                           CrossSegment::TopLeft,
                                           +bleed,
                                           -bleed);
                        draw_cross_at_grid(front_page,
                                           x,
                                           y + 1,
                                           CrossSegment::BottomLeft,
                                           +bleed,
                                           +bleed);
                    }
                }
            }
        }

        if (project.BacksideEnabled)
        {
            static constexpr const char render_fmt[]{
                "Rendering backside for page {}...\nImage number {} - {}"
            };

            HPDF_Page back_page{ HPDF_AddPage(pdf) };
            HPDF_Page_SetWidth(back_page, ToReal(page_size.x));
            HPDF_Page_SetHeight(back_page, ToReal(page_size.y));

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

    const fs::path pdf_path{ fs::path{ project.FileName }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    PPP_LOG("Saving to {}...", pdf_path_string);
    HPDF_SaveToFile(pdf, pdf_path_string.c_str());

    HPDF_Free(pdf);

    return pdf_path;
}
