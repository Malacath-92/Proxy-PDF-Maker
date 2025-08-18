#pragma once

#include <optional>
#include <vector>

#include <dla/vector.h>

#include <ppp/image.hpp>

class Project;

struct PageImageTransform
{
    Position m_Position;
    Size m_Size;
    Image::Rotation m_Rotation;
};
using PageImageTransforms = std::vector<PageImageTransform>;

struct PageImage
{
    std::reference_wrapper<const fs::path> m_Image;
    bool m_BacksideShortEdge;
};
struct Page
{
    std::vector<PageImage> m_Images;
};

std::optional<Size> LoadPdfSize(const fs::path& pdf_path);

PageImageTransforms ComputeTransforms(const Project& project);
PageImageTransforms ComputeBacksideTransforms(
    const Project& project,
    const PageImageTransforms& frontside_transforms);

std::vector<Page> DistributeCardsToPages(const Project& project);
std::vector<Page> MakeBacksidePages(const Project& project, const std::vector<Page>& pages);

// Image::Rotation GetCardRotation(bool is_backside, bool is_short_edge);
