#include <ppp/pdf/util.hpp>

#include <ranges>

#include <podofo/podofo.h>

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

std::vector<Page> DistributeCardsToPages(const Project& project, uint32_t columns, uint32_t rows)
{
    const auto images_per_page{ columns * rows };

    // throw all images N times into a list
    struct TempImageData
    {
        std::reference_wrapper<const fs::path> m_Image;
        bool m_BacksideShortEdge;
    };
    std::vector<TempImageData> images;
    for (const auto& [img, info] : project.Data.Cards)
    {
        for (uint32_t i = 0; i < info.m_Num; i++)
        {
            images.push_back({
                img,
                info.m_BacksideShortEdge,
            });
        }
    }

    auto page_has_space{
        [=](const Page& page)
        {
            const size_t regular_images{ page.m_Images.size() };
            const size_t single_spaces{ regular_images };
            const size_t free_single_spaces{ images_per_page - single_spaces };
            return free_single_spaces > 0;
        }
    };

    auto is_page_full{
        [=](const Page& page)
        {
            return !page_has_space(page);
        }
    };

    std::vector<Page> pages;
    std::vector<Page> unfinished_pages;

    for (const auto& [img, backside_short_edge] : images)
    {
        // get a page that can fit this card
        auto page_with_space{ std::ranges::find_if(unfinished_pages, page_has_space) };

        // or start a new page if none is available
        if (page_with_space == unfinished_pages.end())
        {
            unfinished_pages.emplace_back();
            page_with_space = unfinished_pages.end() - 1;
        }

        // add image to the page
        page_with_space->m_Images.push_back({ img, backside_short_edge });

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
                project.GetBacksideImage(image.m_Image),
                image.m_BacksideShortEdge
            };
        }
    };

    std::vector<Page> backside_pages;
    for (const Page& page : pages)
    {
        Page& backside_page{ backside_pages.emplace_back() };
        backside_page.m_Images = page.m_Images | std::views::transform(backside_of_image) | std::ranges::to<std::vector>();
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
                                       GridImage>;     // explicit image
    auto card_grid{ std::vector{ rows, std::vector{ columns, TempGridImage{} } } };

    {
        size_t k{ 0 };
        for (const auto& [img, backside_short_edge] : page.m_Images)
        {
            dla::uvec2 coord{ get_coord(k) };
            const auto& [x, y]{ coord.pod() };

            // find an empty slot
            while (!std::holds_alternative<std::monostate>(card_grid[x][y]))
            {
                ++k;
                coord = get_coord(k);
            }

            card_grid[x][y] = GridImage{ img, backside_short_edge };
        }
    }

    Grid clean_card_grid{};
    for (size_t x = 0; x < rows; x++)
    {
        static constexpr auto c_CollapseToOptional{
            [](const TempGridImage& tmp_img)
            {
                struct Visitor
                {
                    std::optional<GridImage> operator()(std::monostate)
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
        clean_card_grid.push_back(card_grid[x] |
                                  std::views::transform(c_CollapseToOptional) |
                                  std::ranges::to<std::vector>());
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

Image::Rotation GetCardRotation(bool is_backside, bool is_short_edge)
{
    if (is_backside && is_short_edge)
    {
        return Image::Rotation::Degree180;
    }
    return Image::Rotation::None;
}
