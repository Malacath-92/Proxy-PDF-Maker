#pragma once

#include <optional>
#include <vector>

#include <dla/vector.h>

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
    Rotation m_Rotation;
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

PageImageTransforms ComputeTransforms(const Project& project,
                                      bool no_crop_mode);
PageImageTransforms ComputeBacksideTransforms(
    const Project& project,
    const PageImageTransforms& frontside_transforms,
    bool no_crop_mode);

std::vector<Page> DistributeCardsToPages(const Project& project);
std::vector<Page> MakeBacksidePages(const Project& project, const std::vector<Page>& pages);

std::string GetPageName(std::string_view pdf_name,
                        size_t page_index,
                        size_t page_amount,
                        const PageImageTransforms& transforms,
                        const Page& page);
