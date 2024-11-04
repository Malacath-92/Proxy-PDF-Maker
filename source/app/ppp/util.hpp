#pragma once

#include <span>
#include <vector>
#include <filesystem>

#include <dla/vector.h>

using Length = dla::length_unit;
using Size = dla::tvec2<Length>;

// clang-format off
constexpr auto operator ""_mm(long double v) { return Length{ float(v * 0.0010L) }; }
constexpr auto operator ""_in(long double v) { return Length{ float(v * 0.0254L) }; }

constexpr auto operator ""_mm(unsigned long long v) { return Length{ float(v * 0.0010L) }; }
constexpr auto operator""_in(unsigned long long v) { return Length{ float(v * 0.0254L) }; }
// clang-format off

template<class FunT>
void ForEachFile(const std::filesystem::path& path, FunT&& fun, const std::span<const std::filesystem::path> extensions = {});
template<class FunT>
void ForEachFolder(const std::filesystem::path& path, FunT&& fun);

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& path, const std::span<const std::filesystem::path> extensions = {});
std::vector<std::filesystem::path> ListFolders(const std::filesystem::path& path);

bool OpenFolder(const std::filesystem::path& path);
bool OpenFile(const std::filesystem::path& path);
bool OpenPath(const std::filesystem::path& path);
