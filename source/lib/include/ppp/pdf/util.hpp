#pragma once

#include <optional>
#include <vector>

#include <dla/vector.h>

#include <ppp/image.hpp>

class Project;

struct PageImage
{
    std::reference_wrapper<const fs::path> Image;
    bool BacksideShortEdge;
};
struct Page
{
    std::vector<PageImage> RegularImages;
    std::vector<PageImage> OversizedImages;
};

struct GridImage
{
    std::reference_wrapper<const fs::path> Image;
    bool Oversized;
    bool BacksideShortEdge;
};
using Grid = std::vector<std::vector<std::optional<GridImage>>>;

std::vector<Page> DistributeCardsToPages(const Project& project, uint32_t columns, uint32_t rows);

std::vector<Page> MakeBacksidePages(const Project& project, const std::vector<Page>& pages);

Grid DistributeCardsToGrid(const Page& page, bool left_to_right, uint32_t columns, uint32_t rows);

dla::uvec2 GetGridCords(uint32_t idx, uint32_t columns, bool left_to_right);

Image::Rotation GetCardRotation(bool is_backside, bool is_oversized, bool is_short_edge);
