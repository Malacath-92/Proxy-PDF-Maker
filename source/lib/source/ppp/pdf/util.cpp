#include <ppp/pdf/util.hpp>

#include <functional>
#include <ranges>

#include <ppp/project/project.hpp>
#include <ppp/util.hpp>

std::vector<Page> DistributeCardsToPages(const Project& project, uint32_t columns, uint32_t rows)
{
    const auto images_per_page{ columns * rows };
    const auto oversized_images_per_page{ (columns / 2) * rows };

    // throw all images N times into a list
    struct TempImageData
    {
        std::reference_wrapper<const fs::path> Image;
        bool Oversized;
        bool BacksideShortEdge;
    };
    std::vector<TempImageData> images;
    for (const auto& [img, info] : project.Data.Cards)
    {
        for (uint32_t i = 0; i < info.Num; i++)
        {
            images.push_back({
                img,
                info.Oversized && project.Data.OversizedEnabled,
                info.BacksideShortEdge,
            });
        }
    }

    // favor filling up with oversized cards first
    std::ranges::sort(images, {}, &TempImageData::Oversized);

    auto page_has_space{
        [=](const Page& page, bool image_is_oversized)
        {
            const size_t regular_images{ page.RegularImages.size() };
            const size_t oversized_images{ page.OversizedImages.size() };
            const size_t single_spaces{ regular_images + 2 * oversized_images };
            const size_t free_single_spaces{ images_per_page - single_spaces };
            if (image_is_oversized)
            {
                const size_t free_double_spaces{ oversized_images_per_page - oversized_images };
                return free_double_spaces > 0 and free_single_spaces > 1;
            }
            return free_single_spaces > 0;
        }
    };

    auto is_page_full{
        [=](const Page& page)
        {
            return !page_has_space(page, false);
        }
    };

    std::vector<Page> pages;
    std::vector<Page> unfinished_pages;

    for (const auto& [img, oversized, backside_short_edge] : images)
    {
        // get a page that can fit this card
        auto page_with_space{ std::ranges::find_if(unfinished_pages, std::bind_back(page_has_space, oversized)) };

        // or start a new page if none is available
        if (page_with_space == unfinished_pages.end())
        {
            unfinished_pages.emplace_back();
            page_with_space = unfinished_pages.end() - 1;
        }

        // add image to the page
        (oversized
             ? page_with_space->OversizedImages
             : page_with_space->RegularImages)
            .push_back({ img, backside_short_edge });

        // push full page into final list
        if (is_page_full(*page_with_space))
        {
            pages.push_back(std::move(*page_with_space));
            unfinished_pages.erase(page_with_space);
        }
    }

    // push all unfinished pages into final list
    pages.insert(pages.end(),
                 std::make_move_iterator(unfinished_pages.begin()),
                 std::make_move_iterator(unfinished_pages.end()));

    return pages;
}

std::vector<Page> MakeBacksidePages(const Project& project, const std::vector<Page>& pages)
{
    auto backside_of_image{
        [&](const PageImage& image)
        {
            return PageImage{
                project.GetBacksideImage(image.Image),
                image.BacksideShortEdge
            };
        }
    };

    std::vector<Page> backside_pages;
    for (const Page& page : pages)
    {
        Page& backside_page{ backside_pages.emplace_back() };
        backside_page.RegularImages = page.RegularImages | std::views::transform(backside_of_image) | std::ranges::to<std::vector>();
        backside_page.OversizedImages = page.OversizedImages | std::views::transform(backside_of_image) | std::ranges::to<std::vector>();
    }

    return backside_pages;
}

Grid DistributeCardsToGrid(const Page& page, bool left_to_right, uint32_t columns, uint32_t rows)
{
    auto get_coord{
        [=](size_t i)
        {
            return GetGridCords(static_cast<uint32_t>(i), columns, left_to_right);
        }
    };

    using TempGridImage = std::variant<std::monostate, // empty
                                       std::nullptr_t, // implicit 2nd half of oversized
                                       GridImage>;     // explicit image
    auto card_grid{ std::vector{ rows, std::vector{ columns, TempGridImage{} } } };

    {
        size_t k{ 0 };
        for (const auto& [img, backside_short_edge] : page.OversizedImages)
        {
            dla::uvec2 coord{ get_coord(k) };
            const auto& [x, y]{ coord.pod() };

            // find slot that fits an oversized card
            while (y + 1 >= columns || !std::holds_alternative<std::monostate>(card_grid[x][y + 1]))
            {
                ++k;
                coord = get_coord(k);
            }

            card_grid[x][y] = GridImage{ img, true, backside_short_edge };
            card_grid[x][y + 1] = nullptr;
            k += 2;
        }
    }

    {
        size_t k{ 0 };
        for (const auto& [img, backside_short_edge] : page.RegularImages)
        {
            dla::uvec2 coord{ get_coord(k) };
            const auto& [x, y]{ coord.pod() };

            // find slot that fits an oversized card
            while (!std::holds_alternative<std::monostate>(card_grid[x][y]))
            {
                ++k;
                coord = get_coord(k);
            }

            card_grid[x][y] = GridImage{ img, false, backside_short_edge };
        }
    }

    Grid clean_card_grid{};
    for (size_t x = 0; x < rows; x++)
    {
        static constexpr auto collapse_to_optional{
            [](const TempGridImage& tmp_img)
            {
                struct Visitor
                {
                    std::optional<GridImage> operator()(std::monostate)
                    {
                        return std::nullopt;
                    }
                    std::optional<GridImage> operator()(std::nullptr_t)
                    {
                        return std::nullopt;
                    }
                    std::optional<GridImage> operator()(const GridImage& img)
                    {
                        return img;
                    }
                };
                return std::visit(Visitor{}, tmp_img);
            }
        };
        clean_card_grid.push_back(card_grid[x] | std::views::transform(collapse_to_optional) | std::ranges::to<std::vector>());
    }

    return clean_card_grid;
}

dla::uvec2 GetGridCords(uint32_t idx, uint32_t columns, bool left_to_right)
{
    const uint32_t x{ idx / columns };
    uint32_t y{ idx % columns };
    if (!left_to_right)
    {
        y = columns - y - 1;
    }
    return { x, y };
}

Image::Rotation GetCardRotation(bool is_backside, bool is_oversized, bool is_short_edge)
{
    if (is_backside)
    {
        if (is_short_edge)
        {
            if (is_oversized)
            {
                return Image::Rotation::Degree90;
            }
            return Image::Rotation::Degree180;
        }
        else if (is_oversized)
        {
            return Image::Rotation::Degree270;
        }
    }
    else if (is_oversized)
    {
        return Image::Rotation::Degree90;
    }

    return Image::Rotation::None;
}
