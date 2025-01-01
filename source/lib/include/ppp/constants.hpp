#pragma once

#include <string_view>
#include <unordered_map>

#include <hpdf_types.h>

#include <ppp/util.hpp>

fs::path cwd();

inline constexpr Size CardSizeWithBleed{ 2.72_in, 3.7_in };
inline constexpr Size CardSizeWithoutBleed{ 2.48_in, 3.46_in };
inline constexpr float CardRatio{ CardSizeWithoutBleed.x / CardSizeWithoutBleed.y };
