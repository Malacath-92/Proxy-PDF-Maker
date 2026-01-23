#include <ppp/pdf/util.hpp>

#include <ranges>

#include <podofo/podofo.h>

#include <dla/vector_math.h>

#include <ppp/project/project.hpp>
#include <ppp/util.hpp>

std::optional<Size> LoadPdfSize(const fs::path& pdf_path)
{
    const fs::path full_path{ "./res/base_pdfs" / pdf_path };
    if (fs::exists(full_path))
    {
        PoDoFo::PdfMemDocument document;
        document.Load(full_path.string());

        const auto& pages{ document.GetPages() };
        if (pages.GetCount() > 0)
        {
            const PoDoFo::PdfPage& page{ pages.GetPageAt(0) };
            const PoDoFo::Rect rect{ page.GetRect() };
            const Length width{ static_cast<float>(rect.Width) * 1_pts };
            const Length height{ static_cast<float>(rect.Height) * 1_pts };
            return Size{ width, height };
        }
    }
    return std::nullopt;
}

void GenerateEnvelope(std::span<PageImageTransform> transforms,
                      Length envelope_size)
{
    if (envelope_size <= 0_mm)
    {
        return;
    }

    for (PageImageTransform& transform : transforms)
    {
        transform.m_Position -= envelope_size;
        transform.m_Size += envelope_size * 2;
        transform.m_ClipRect = {
            transform.m_Position,
            transform.m_Size,
        };
    }

    // O(N^2) approach, should be decent for the amount of cards
    // typically on a single page
    for (size_t i{ 0 }; i < transforms.size(); ++i)
    {
        const auto& lhs_size{ transforms[i].m_Size };
        const auto lhs_half_size{ lhs_size / 2 };
        const auto& lhs_center{ transforms[i].m_Position + lhs_half_size };

        auto& lhs_clip{ transforms[i].m_ClipRect.value() };
        for (size_t j{ i + 1 }; j < transforms.size(); ++j)
        {
            const auto& rhs_size{ transforms[j].m_Size };
            const auto rhs_half_size{ rhs_size / 2 };
            const auto& rhs_center{ transforms[j].m_Position + rhs_half_size };

            auto& rhs_clip{ transforms[j].m_ClipRect.value() };

            const auto distance_manhattan{ dla::abs(lhs_center - rhs_center) };
            const auto intersection{ lhs_half_size + rhs_half_size - distance_manhattan };
            if (intersection.x > 0_mm && intersection.y > 0_mm)
            {
                // Intersecting ...
                if (dla::math::abs(distance_manhattan.x) > dla::math::abs(distance_manhattan.y))
                {
                    // ... clipping the x-direction ...
                    const auto middle_x{ (lhs_center.x + rhs_center.x) / 2 };
                    if (middle_x > lhs_center.x)
                    {
                        // ... right of lhs (left of rhs) ...
                        lhs_clip.m_Size.x = middle_x - lhs_clip.m_Position.x;
                        rhs_clip.m_Size.x -= middle_x - rhs_clip.m_Position.x;
                        rhs_clip.m_Position.x = middle_x;
                    }
                    else
                    {
                        // ... left of lhs (right of rhs) ...
                        lhs_clip.m_Size.x -= middle_x - lhs_clip.m_Position.x;
                        lhs_clip.m_Position.x = middle_x;
                        rhs_clip.m_Size.x = middle_x - rhs_clip.m_Position.x;
                    }
                }
                else
                {
                    // or the y-direction ...
                    const auto middle_y{ (lhs_center.y + rhs_center.y) / 2 };
                    if (middle_y > lhs_center.y)
                    {
                        // ... above lhs (below rhs) ...
                        lhs_clip.m_Size.y = middle_y - lhs_clip.m_Position.y;
                        rhs_clip.m_Size.y -= middle_y - rhs_clip.m_Position.y;
                        rhs_clip.m_Position.y = middle_y;
                    }
                    else
                    {
                        // ... below lhs (above rhs) ...
                        lhs_clip.m_Size.y -= middle_y - lhs_clip.m_Position.y;
                        lhs_clip.m_Position.y = middle_y;
                        rhs_clip.m_Size.y = middle_y - rhs_clip.m_Position.y;
                    }
                }
            }
        }
    }
}

PageImageTransforms ComputeTransforms(const Project& project)
{
    const auto layout_vertical{ project.m_Data.m_CardLayoutVertical };
    const auto layout_horizontal{ project.m_Data.m_CardLayoutHorizontal };

    // Return empty result when no cards can fit on a page (invalid layout)
    if ((layout_vertical.x == 0 || layout_vertical.y == 0) &&
        (layout_horizontal.x == 0 || layout_horizontal.y == 0))
    {
        return {};
    }

    const auto card_size_vertical_no_bleed{ project.CardSize() };
    const auto card_size_horizontal_no_bleed{ dla::rotl(card_size_vertical_no_bleed) };
    const auto card_size_vertical{ project.CardSizeWithBleed() };
    const auto card_size_horizontal{ dla::rotl(card_size_vertical) };
    const auto cards_size{ project.ComputeCardsSize() };
    const auto cards_size_vertical{ project.ComputeCardsSizeVertical() };
    const auto cards_size_horizontal{ project.ComputeCardsSizeHorizontal() };
    const auto envelope_bleed{ project.m_Data.m_EnvelopeBleedEdge };
    const auto cards_size_width_offset{ (cards_size_vertical.x - cards_size_horizontal.x) / 2 };
    const auto origin_width_vertical{ cards_size_width_offset > 0_mm ? 0_mm : -cards_size_width_offset };
    const auto origin_width_horizontal{ cards_size_width_offset < 0_mm ? 0_mm : cards_size_width_offset };
    const auto page_size{ project.ComputePageSize() };
    const auto margins{ project.ComputeMargins() };
    const Size margins_size{
        page_size.x - margins.m_Left - margins.m_Right,
        page_size.y - margins.m_Top - margins.m_Bottom,
    };

    const Position cards_origin{
        margins.m_Left + (margins_size.x - cards_size.x) / 2.0f + envelope_bleed,
        margins.m_Top + (margins_size.y - cards_size.y) / 2.0f + envelope_bleed,
    };
    const Position origin_vertical{
        cards_origin.x + origin_width_vertical,
        cards_origin.y,
    };
    const Position origin_horizontal{
        cards_origin.x + origin_width_horizontal,
        cards_origin.y + cards_size_vertical.y +
            project.m_Data.m_Spacing.y,
    };

    const auto vertical_images_per_page{ layout_vertical.x * layout_vertical.y };
    const auto horizontal_images_per_page{ layout_horizontal.x * layout_horizontal.y };
    const auto images_per_page{ vertical_images_per_page + horizontal_images_per_page };

    PageImageTransforms transforms{};
    transforms.reserve(images_per_page);

    for (auto i = 0u; i < vertical_images_per_page; ++i)
    {
        const auto origin{ origin_vertical };
        const auto columns{ layout_vertical.x };
        const dla::uvec2 grid_pos{ i % columns, i / columns };
        const auto position{ origin + grid_pos * card_size_vertical + grid_pos * project.m_Data.m_Spacing };
        transforms.push_back(PageImageTransform{
            .m_Position{ position },
            .m_Size{ card_size_vertical },
            .m_Rotation = Image::Rotation::None,
            .m_Card{
                .m_Position{ position + project.m_Data.m_BleedEdge },
                .m_Size{ card_size_vertical_no_bleed },
            },
            .m_ClipRect{ std::nullopt },
        });
    }

    for (auto i = 0u; i < horizontal_images_per_page; ++i)
    {
        const auto origin{ origin_horizontal };
        const auto columns{ layout_horizontal.x };
        const dla::uvec2 grid_pos{ i % columns, i / columns };
        const auto position{ origin + grid_pos * card_size_horizontal + grid_pos * project.m_Data.m_Spacing };
        transforms.push_back(PageImageTransform{
            .m_Position{ position },
            .m_Size{ card_size_horizontal },
            .m_Rotation = Image::Rotation::Degree90,
            .m_Card{
                .m_Position{ position + project.m_Data.m_BleedEdge },
                .m_Size{ card_size_horizontal_no_bleed },
            },
            .m_ClipRect{ std::nullopt },
        });
    }

    if (project.m_Data.m_EnvelopeBleedEdge > 0_mm)
    {
        GenerateEnvelope(
            std::span{
                transforms.data(),
                vertical_images_per_page,
            },
            project.m_Data.m_EnvelopeBleedEdge);

        GenerateEnvelope(
            std::span{
                transforms.data() + vertical_images_per_page,
                horizontal_images_per_page,
            },
            project.m_Data.m_EnvelopeBleedEdge);
    }

    return transforms;
}

PageImageTransforms ComputeBacksideTransforms(
    const Project& project,
    const PageImageTransforms& frontside_transforms)
{
    if (frontside_transforms.empty())
    {
        return {};
    }

    const auto page_size{ project.ComputePageSize() };

    const auto flip_on_left{ project.m_Data.m_FlipOn == FlipPageOn::LeftEdge };
    auto get_backside_rotation{
        [flip_on_left](Image::Rotation frontside_rotation) -> Image::Rotation
        {
            switch (frontside_rotation)
            {
            default:
            case Image::Rotation::None:
                return flip_on_left ? Image::Rotation::None : Image::Rotation::Degree180;
            case Image::Rotation::Degree90:
                return flip_on_left ? Image::Rotation::Degree270 : Image::Rotation::Degree90;
            case Image::Rotation::Degree180:
                return flip_on_left ? Image::Rotation::Degree180 : Image::Rotation::None;
            case Image::Rotation::Degree270:
                return flip_on_left ? Image::Rotation::Degree90 : Image::Rotation::Degree270;
            }
        }
    };

    const auto backside_offset{ project.m_Data.m_BacksideOffset };
    const auto envelope_size{ project.m_Data.m_EnvelopeBleedEdge };

    PageImageTransforms backside_transforms;
    backside_transforms.reserve(frontside_transforms.size());

    for (const PageImageTransform& transform : frontside_transforms)
    {
        const auto& frontside_size{ transform.m_Size - envelope_size * 2 };
        const auto backside_position{
            [&]
            {
                const auto& frontside_position{ transform.m_Position + envelope_size };

                if (flip_on_left)
                {
                    const auto backside_position_x{ page_size.x -
                                                    frontside_position.x -
                                                    frontside_size.x -
                                                    backside_offset.x };
                    const auto backside_position_y{ frontside_position.y -
                                                    backside_offset.y };
                    return Position{
                        backside_position_x,
                        backside_position_y,
                    };
                }
                else
                {
                    const auto backside_position_x{ frontside_position.x -
                                                    backside_offset.x };
                    const auto backside_position_y{ page_size.y -
                                                    frontside_position.y -
                                                    frontside_size.y -
                                                    backside_offset.y };
                    return Position{
                        backside_position_x,
                        backside_position_y,
                    };
                }
            }(),
        };

        backside_transforms.push_back(PageImageTransform{
            .m_Position{ backside_position },
            .m_Size{ frontside_size },
            .m_Rotation = get_backside_rotation(transform.m_Rotation),
            .m_Card{
                .m_Position{ backside_position + project.m_Data.m_BleedEdge },
                .m_Size{ frontside_size },
            },
            .m_ClipRect{ std::nullopt },
        });
    }

    if (envelope_size > 0_mm)
    {
        const auto layout_vertical{ project.m_Data.m_CardLayoutVertical };
        const auto layout_horizontal{ project.m_Data.m_CardLayoutHorizontal };

        const auto vertical_images_per_page{ layout_vertical.x * layout_vertical.y };
        const auto horizontal_images_per_page{ layout_horizontal.x * layout_horizontal.y };

        GenerateEnvelope(
            std::span{
                backside_transforms.data(),
                vertical_images_per_page,
            },
            envelope_size);

        GenerateEnvelope(
            std::span{
                backside_transforms.data() + vertical_images_per_page,
                horizontal_images_per_page,
            },
            envelope_size);
    }

    return backside_transforms;
}

std::vector<Page> DistributeCardsToPages(const Project& project)
{
    const auto layout_vertical{ project.m_Data.m_CardLayoutVertical };
    const auto layout_horizontal{ project.m_Data.m_CardLayoutHorizontal };

    // Return empty result when no cards can fit on a page (invalid layout)
    if ((layout_vertical.x == 0 || layout_vertical.y == 0) &&
        (layout_horizontal.x == 0 || layout_horizontal.y == 0))
    {
        return {};
    }

    const auto vertical_images_per_page{ layout_vertical.x * layout_vertical.y };
    const auto horizontal_images_per_page{ layout_horizontal.x * layout_horizontal.y };
    const auto images_per_page{ vertical_images_per_page + horizontal_images_per_page };

    std::vector<Page> pages;
    pages.emplace_back();

    auto push_card{
        [&, index = size_t{ 0 }](const auto& info) mutable
        {
            // make new page if last page is full
            if (pages.back().m_Images.size() == images_per_page)
            {
                pages.emplace_back();
            }

            const auto& img{ info.m_Name };

            // add image to the page
            Page& page{ pages.back() };
            page.m_Images.push_back({
                img,
                info.m_BacksideShortEdge,
                index++,
            });
        }
    };

    if (project.IsManuallySorted())
    {
        // Pre-expanded list of cards with custom sorting
        for (const auto& img : project.GetManualSorting())
        {
            if (const auto* info{ project.FindCard(img) })
            {
                if (info->m_Hidden > 0 || info->m_Transient)
                {
                    continue;
                }

                push_card(*info);
            }
        }
    }
    else
    {
        for (const auto& info : project.GetCards())
        {
            if (info.m_Hidden > 0 || info.m_Transient)
            {
                continue;
            }

            for (uint32_t i = 0; i < info.m_Num; i++)
            {
                push_card(info);
            }
        }
    }

    return pages;
}

std::vector<Page> MakeBacksidePages(const Project& project, const std::vector<Page>& pages)
{
    auto backside_of_image{
        [&](const PageImage& image)
        {
            return PageImage{
                project.GetBacksideImage(image.m_Image),
                image.m_BacksideShortEdge,
                image.m_Index,
            };
        }
    };

    std::vector<Page> backside_pages;
    backside_pages.reserve(pages.size());

    for (const Page& page : pages)
    {
        Page& backside_page{ backside_pages.emplace_back() };
        backside_page.m_Images = page.m_Images |
                                 std::views::transform(backside_of_image) |
                                 std::ranges::to<std::vector>();
    }

    return backside_pages;
}
