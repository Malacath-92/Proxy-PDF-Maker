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
        document.Load(full_path.c_str());

        if (document.GetPageCount() > 0)
        {
            const PoDoFo::PdfPage* page{ document.GetPage(0) };
            const PoDoFo::PdfRect rect{ page->GetPageSize() };
            const Length width{ static_cast<float>(rect.GetWidth()) * 1_pts };
            const Length height{ static_cast<float>(rect.GetHeight()) * 1_pts };
            return Size{ width, height };
        }
    }
    return std::nullopt;
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

    const auto card_size_vertical{ project.CardSizeWithBleed() };
    const auto card_size_horizontal{ dla::rotl(project.CardSizeWithBleed()) };
    const auto cards_size{ project.ComputeCardsSize() };
    const auto cards_size_vertical{ project.ComputeCardsSizeVertical() };
    const auto cards_size_horizontal{ project.ComputeCardsSizeHorizontal() };
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
        margins.m_Left + (margins_size.x - cards_size.x) / 2.0f,
        margins.m_Top + (margins_size.y - cards_size.y) / 2.0f,
    };
    const Position origin_vertical{
        cards_origin.x + origin_width_vertical,
        cards_origin.y,
    };
    const Position origin_horizontal{
        cards_origin.x + origin_width_horizontal,
        cards_origin.y + cards_size_vertical.y + project.m_Data.m_Spacing.y,
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
        transforms.push_back(PageImageTransform{ position, card_size_vertical, Image::Rotation::None });
    }

    for (auto i = 0u; i < horizontal_images_per_page; ++i)
    {
        const auto origin{ origin_horizontal };
        const auto columns{ layout_horizontal.x };
        const dla::uvec2 grid_pos{ i % columns, i / columns };
        const auto position{ origin + grid_pos * card_size_horizontal + grid_pos * project.m_Data.m_Spacing };
        transforms.push_back(PageImageTransform{ position, card_size_horizontal, Image::Rotation::Degree90 });
    }

    return transforms;
}

PageImageTransforms ComputeBacksideTransforms(
    const Project& project,
    const PageImageTransforms& frontside_transforms)
{
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

    PageImageTransforms backside_transforms;
    backside_transforms.reserve(frontside_transforms.size());

    for (const PageImageTransform& transform : frontside_transforms)
    {
        const auto& frontside_position{ transform.m_Position };
        const auto& frontside_size{ transform.m_Size };
        if (flip_on_left)
        {
            const auto backside_position_x{ page_size.x -
                                            frontside_position.x -
                                            frontside_size.x -
                                            backside_offset.x };
            const auto backside_position_y{ frontside_position.y -
                                            backside_offset.y };
            const Position backside_position{
                backside_position_x,
                backside_position_y,
            };
            backside_transforms.push_back(PageImageTransform{
                backside_position,
                frontside_size,
                get_backside_rotation(transform.m_Rotation),
            });
        }
        else
        {
            const auto backside_position_x{ frontside_position.x -
                                            backside_offset.x };
            const auto backside_position_y{ page_size.y -
                                            frontside_position.y -
                                            frontside_size.y -
                                            backside_offset.y };
            const Position backside_position{
                backside_position_x,
                backside_position_y,
            };
            backside_transforms.push_back(PageImageTransform{
                backside_position,
                frontside_size,
                get_backside_rotation(transform.m_Rotation),
            });
        }
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
                push_card(*info);
            }
        }
    }
    else
    {
        for (const auto& info : project.GetCards())
        {
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
