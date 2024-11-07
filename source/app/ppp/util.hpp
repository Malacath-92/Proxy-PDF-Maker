#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include <dla/vector.h>

namespace fs = std::filesystem;

using Length = dla::length_unit;
using Size = dla::tvec2<Length>;

// clang-format off
constexpr auto operator ""_mm(long double v) { return Length{ float(v * 0.0010L) }; }
constexpr auto operator ""_in(long double v) { return Length{ float(v * 0.0254L) }; }

constexpr auto operator ""_mm(unsigned long long v) { return Length{ float(v * 0.0010L) }; }
constexpr auto operator""_in(unsigned long long v) { return Length{ float(v * 0.0254L) }; }

inline auto operator ""_p(const char *str, size_t len) { return fs::path(str, str + len); }
inline auto operator ""_p(const wchar_t *str, size_t len) { return fs::path(str, str + len); }
inline auto operator ""_p(const char16_t *str, size_t len) { return fs::path(str, str + len); }
inline auto operator ""_p(const char32_t *str, size_t len) { return fs::path(str, str + len); }
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
