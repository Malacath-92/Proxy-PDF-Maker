#pragma once

#include <string_view>
#include <unordered_map>

#include <hpdf_types.h>

#include <ppp/util.hpp>

fs::path cwd();

inline const std::unordered_map<std::string_view, HPDF_PageSizes> PageSizes{
    { "Letter", HPDF_PAGE_SIZE_LETTER },
    { "A5", HPDF_PAGE_SIZE_A5 },
    { "A4", HPDF_PAGE_SIZE_A4 },
    { "A3", HPDF_PAGE_SIZE_A3 },
    { "Legal", HPDF_PAGE_SIZE_LEGAL },
};

inline constexpr Size PredefinedPageSizes[]{
    { 612_pts, 792_pts },
    { 612_pts, 1008_pts },
    { 841.89_pts, 1190.551_pts },
    { 595.276_pts, 841.89_pts },
    { 419.528_pts, 595.276_pts },
    { 708.661_pts, 1000.63_pts },
    { 498.898_pts, 708.661_pts },
    { 522_pts, 756_pts },
    { 288_pts, 432_pts },
    { 288_pts, 576_pts },
    { 360_pts, 504_pts },
    { 297_pts, 684_pts }
};

inline constexpr Size CardSizeWithBleed{ 2.72_in, 3.7_in };
inline constexpr Size CardSizeWithoutBleed{ 2.48_in, 3.46_in };
inline constexpr float CardRatio{ CardSizeWithoutBleed.x / CardSizeWithoutBleed.y };
