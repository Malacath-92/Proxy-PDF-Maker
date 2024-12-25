#pragma once

#include <cstdint>
#include <string>

#include <dla/vector.h>

using ColorRGB8 = dla::tvec3<uint8_t>;
using ColorRGB32f = dla::tvec3<float>;

uint32_t ColorToInt(const ColorRGB8& color);
std::string ColorToHex(const ColorRGB8& color);
