#include <ppp/color.hpp>

#include <fmt/format.h>

uint32_t ColorToInt(const ColorRGB8& color)
{
    return static_cast<uint32_t>((color.r << 16) | (color.g << 8) | color.b);
}

std::string ColorToHex(const ColorRGB8& color)
{
    return fmt::format("{:2>#x}{:2>#x}{:2>#x}", color.r, color.g, color.b);
}