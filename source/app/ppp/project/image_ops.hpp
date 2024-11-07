#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

using PrintFn = std::function<void(std::string_view)>;
using ImgDict = std::unordered_map<std::string, Image>;

void InitImageSystem(const char* program_name);
void InitFolders(const fs::path& image_dir, const fs::path& crop_dir);

template<class FunT>
void ForEachImageFile(const fs::path& path, FunT&& fun);

std::vector<fs::path> ListImageFiles(const fs::path& path);

Image CropImage(const Image& image, std::string_view image_name, Length bleed_edge, PixelDensity max_density, PrintFn print_fn = nullptr);
Image UncropImage(const Image& image, std::string_view image_name, PrintFn print_fn = nullptr);

bool NeedRunCropper(const fs::path& image_dir, const fs::path& crop_dir, Length bleed_edge, bool do_vibrance_bump);
ImgDict RunCropper(const fs::path& image_dir,
                   const fs::path& crop_dir,
                   const fs::path& img_cache,
                   const ImgDict& img_dict,
                   Length bleed_edge,
                   PixelDensity max_density,
                   bool do_vibrance_bump,
                   bool uncrop,
                   PrintFn print_fn);

// def image_from_bytes(bytes);
// def image_to_bytes(img);
// def to_bytes(file_or_bytes, resize=None);

bool NeedCachePreviews(const fs::path& crop_dir, ImgDict& img_dict);
ImgDict CachePreviews(const fs::path& img_cache_file, const fs::path& image_dir, const fs::path& crop_dir, PrintFn print_fn, const ImgDict& img_dict);
