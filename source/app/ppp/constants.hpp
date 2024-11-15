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

inline constexpr Size CardSizeWithBleed{ 2.72_in, 3.7_in };
inline constexpr Size CardSizeWithoutBleed{ 2.48_in, 3.46_in };
inline constexpr float CardRatio{ CardSizeWithoutBleed.x / CardSizeWithoutBleed.y };
