#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include <dla/literals.h>
#include <dla/vector.h>

namespace fs = std::filesystem;

using Length = dla::length_unit;

namespace dla::unit_name
{
struct pixel
{
    static constexpr const char* id = "pixels";
    static constexpr const char* symbol = "pixels";
};
} // namespace dla::unit_name
using pixel_tag = dla::unit_tag<dla::unit_name::pixel>;
using Pixel = dla::base_unit<pixel_tag>;

using Size = dla::tvec2<Length>;
using PixelSize = dla::tvec2<Pixel>;
using PixelDensity = decltype(Pixel{} / Length{});

// clang-format off
using namespace dla::literals;
using namespace dla::int_literals;

constexpr auto operator""_mm(long double v) { return Length{ float(v * 0.001L) }; }
constexpr auto operator""_mm(unsigned long long v) { return Length{ float(v * 0.001L) }; }

constexpr auto operator""_cm(long double v) { return Length{ float(v * 0.01L) }; }
constexpr auto operator""_cm(unsigned long long v) { return Length{ float(v * 0.01L) }; }

constexpr auto operator""_in(long double v) { return Length{ float(v * 0.0254L) }; }
constexpr auto operator""_in(unsigned long long v) { return Length{ float(v * 0.0254L) }; }

constexpr auto operator""_pts(long double v) { return 0.0138889_in * float(v); }
constexpr auto operator""_pts(unsigned long long v) { return 0.0138889_in * float(v); }

constexpr auto operator""_dpi(long double v) { return Pixel(float(v)) / 1_in; }
constexpr auto operator""_dpi(unsigned long long v) { return Pixel{ float(v) } / 1_in; }

constexpr auto operator""_pix(long double v) { return Pixel(float(v)); }
constexpr auto operator""_pix(unsigned long long v) { return Pixel{ float(v) }; }

inline auto operator""_p(const char *str, size_t len) { return fs::path(str, str + len); }
inline auto operator""_p(const wchar_t *str, size_t len) { return fs::path(str, str + len); }
inline auto operator""_p(const char16_t *str, size_t len) { return fs::path(str, str + len); }
inline auto operator""_p(const char32_t *str, size_t len) { return fs::path(str, str + len); }

inline constexpr Length GetLengthUnit(std::string_view name)
{
    if (name == "inches")
    {
        return 1_in;
    }
    else if (name == "points")
    {
        return 1_pts;
    }
    else if (name == "cm")
    {
        return 10_mm;
    }
    else
    {
        return 1_mm;
    }
}
inline constexpr const char* GetLengthUnitName(Length base)
{
    const bool is_inches{ dla::math::abs(base - 1_in) < 0.0001_in };
    const bool is_cm{ dla::math::abs(base - 10_mm) < 0.0001_mm };
    const bool is_mm{ dla::math::abs(base - 1_mm) < 0.00001_mm };
    const bool is_pts{ dla::math::abs(base - 1_pts) < 0.0001_pts };
    // clang-format off
    return is_inches ? "inches" :
               is_cm ? "cm" :
               is_mm ? "mm" :
              is_pts ? "points"
                     : "??";
    // clang-format on
}
inline constexpr const char* GetLengthUnitShortName(Length base)
{
    const bool is_inches{ dla::math::abs(base - 1_in) < 0.0001_in };
    const bool is_cm{ dla::math::abs(base - 10_mm) < 0.0001_mm };
    const bool is_mm{ dla::math::abs(base - 1_mm) < 0.00001_mm };
    const bool is_pts{ dla::math::abs(base - 1_pts) < 0.0001_pts };
    // clang-format off
    return is_inches ? "in" :
               is_cm ? "cm" :
               is_mm ? "mm" :
              is_pts ? "pts"
                     : "??";
    // clang-format on
}

template<class T>
struct TagT
{
};
template<class T>
inline constexpr TagT<T> Tag{};
// clang-format off

template<class FunT>
void ForEachFile(const fs::path& path, FunT&& fun, const std::span<const fs::path> extensions)
{
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    std::vector<fs::path> files;
    for (auto& child : std::filesystem::directory_iterator(path))
    {
        if (!child.is_directory())
        {
            const bool is_matching_extension{
                extensions.empty() || std::ranges::contains(extensions, child.path().extension())
            };
            if (is_matching_extension)
            {
                fun(child.path());
            }
        }
    }
}
template<class FunT>
void ForEachFolder(const fs::path& path, FunT&& fun)
{
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    std::vector<fs::path> files;
    for (auto& child : std::filesystem::directory_iterator(path))
    {
        if (child.is_directory())
        {
            fun(child.path());
        }
    }
}

std::vector<fs::path> ListFiles(const fs::path& path, const std::span<const fs::path> extensions = {});
std::vector<fs::path> ListFolders(const fs::path& path);

bool OpenFolder(const fs::path& path);
bool OpenFile(const fs::path& path);
bool OpenPath(const fs::path& path);

template<class FunT>
struct AtScopeExit
{
    ~AtScopeExit()
    {
        Dtor();
    }
    FunT Dtor;
};
