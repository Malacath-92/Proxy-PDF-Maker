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

struct ImagePreview
{
    Image CroppedImage;
    Image UncroppedImage;
    Image CroppedThumbImage;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

void InitImageSystem(const char* program_name);
void InitFolders(const fs::path& image_dir, const fs::path& crop_dir);

template<class FunT>
void ForEachImageFile(const fs::path& path, FunT&& fun);

std::vector<fs::path> ListImageFiles(const fs::path& path);

Image CropImage(const Image& image, const fs::path& image_name, Length bleed_edge, PixelDensity max_density, PrintFn print_fn = nullptr);
Image UncropImage(const Image& image, const fs::path& image_name, PrintFn print_fn = nullptr);

bool NeedRunCropper(const fs::path& image_dir, const fs::path& crop_dir, Length bleed_edge, bool do_vibrance_bump);
ImgDict RunCropper(const fs::path& image_dir,
                   const fs::path& crop_dir,
                   const fs::path& img_cache_file,
                   const ImgDict& img_dict,
                   Length bleed_edge,
                   PixelDensity max_density,
                   bool do_vibrance_bump,
                   bool uncrop,
                   PrintFn print_fn);

bool NeedCachePreviews(const fs::path& crop_dir, const ImgDict& img_dict);
ImgDict CachePreviews(const fs::path& image_dir, const fs::path& crop_dir, const fs::path& img_cache_file, const ImgDict& img_dict, PrintFn print_fn);

ImgDict ReadPreviews(const fs::path& img_cache_file);
void WritePreviews(const fs::path& img_cache_file, const ImgDict& img_dict);
