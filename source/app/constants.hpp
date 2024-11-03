#pragma once

#include <string_view>

#include <dla/literals.h>
#include <dla/vector.h>

std::string_view cwd();

using Size = dla::tvec2<dla::length_unit>;

//page_sizes = {"Letter": LETTER, "A5": A5, "A4": A4, "A3": A3, "Legal": LEGAL}

constexpr auto operator ""_mm(long double v) { return dla::length_unit{ float(v / 1000.0L) }; }
constexpr auto operator ""_in(long double v) { return dla::length_unit{ float(v * 0.0254L) }; }

inline constexpr Size card_size_with_bleed{ 2.72_in, 3.7_in };
inline constexpr Size card_size_without_bleed{ 2.48_in, 3.46_in };
inline constexpr float card_ratio{ card_size_without_bleed.x / card_size_without_bleed.y };
