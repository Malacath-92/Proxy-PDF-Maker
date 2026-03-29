#pragma once

#include <optional>
#include <vector>

#include <dla/vector.h>

#include <ppp/image.hpp>
#include <ppp/project/card_info.hpp>

class Project;

struct CardTransform
{
    Position m_Position;
    Size m_Size;
};

struct ClipRect
{
    Position m_Position;
    Size m_Size;
};

struct PageImageTransform
{
    Position m_Position;
    Size m_Size;
    Image::Rotation m_Rotation;
    CardTransform m_Card;
    std::optional<ClipRect> m_ClipRect;
};
using PageImageTransforms = std::vector<PageImageTransform>;

struct PageImage
{
    OptionalImageRef m_Image;
    bool m_BacksideShortEdge;
    size_t m_Index;
    size_t m_Slot;
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
