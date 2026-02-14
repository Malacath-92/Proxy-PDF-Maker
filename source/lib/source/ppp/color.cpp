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

CategorizedColors CategorizeColors(const ColorRGB8& lhs, const ColorRGB8& rhs)
{
    const auto get_luminosity{
        [](const ColorRGB8& col)
        {
            const auto& min{ dla::min_element(col) };
            const auto& max{ dla::max_element(col) };
            return (min + max) / 2;
        }
    };
    const auto color_a_luminosity{ get_luminosity(lhs) };
    const auto color_b_luminosity{ get_luminosity(rhs) };
    const bool color_a_darker{ color_a_luminosity < color_b_luminosity };

    if (color_a_darker)
    {
        return CategorizedColors{ lhs, rhs };
    }
    return CategorizedColors{ rhs, lhs };
}
