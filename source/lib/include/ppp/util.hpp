#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <vector>

#include <dla/literals.h>
#include <dla/vector.h>

namespace fs = std::filesystem;

using PrintFn = std::function<void(std::string_view)>;

#define PPP_LOG(fmt_str, ...)                              \
    do                                                     \
    {                                                      \
        if (print_fn != nullptr)                           \
        {                                                  \
            print_fn(fmt::format(fmt_str, ##__VA_ARGS__)); \
        }                                                  \
    } while (false)

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

constexpr auto operator""_mm(long double v) { return Length{ float(v * 0.0010L) }; }
constexpr auto operator""_mm(unsigned long long v) { return Length{ float(v * 0.0010L) }; }

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

template<class T>
struct TagT{};
template<class T>
inline constexpr TagT<T> Tag{};
// clang-format off

template<class FunT>
void ForEachFile(const fs::path& path, FunT&& fun, const std::span<const fs::path> extensions = {});
template<class FunT>
void ForEachFolder(const fs::path& path, FunT&& fun);

std::vector<fs::path> ListFiles(const fs::path& path, const std::span<const fs::path> extensions = {});
std::vector<fs::path> ListFolders(const fs::path& path);

bool OpenFolder(const fs::path& path);
bool OpenFile(const fs::path& path);
bool OpenPath(const fs::path& path);