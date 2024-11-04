#pragma once

#include <string_view>

#include <ppp/util.hpp>

std::string_view cwd();

//page_sizes = {"Letter": LETTER, "A5": A5, "A4": A4, "A3": A3, "Legal": LEGAL}

inline constexpr Size card_size_with_bleed{ 2.72_in, 3.7_in };
inline constexpr Size card_size_without_bleed{ 2.48_in, 3.46_in };
inline constexpr float card_ratio{ card_size_without_bleed.x / card_size_without_bleed.y };
