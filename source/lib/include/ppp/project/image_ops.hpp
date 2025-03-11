#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

namespace cv
{
class Mat;
}

inline const std::array ValidImageExtensions{
    ".bmp"_p,
    ".gif"_p,
    ".jpg"_p,
    ".jpeg"_p,
    ".png"_p,
};

void InitFolders(const fs::path& image_dir, const fs::path& crop_dir);

std::vector<fs::path> ListImageFiles(const fs::path& path);
std::vector<fs::path> ListImageFiles(const fs::path& path_one, const fs::path& path_two);

Image CropImage(const Image& image, const fs::path& image_name, Length bleed_edge, PixelDensity max_density, PrintFn print_fn = nullptr);
Image UncropImage(const Image& image, const fs::path& image_name, PrintFn print_fn = nullptr);

fs::path GetOutputDir(const fs::path& crop_dir, Length bleed_edge, const std::string& color_cube_name);

bool NeedRunCropper(const fs::path& image_dir, const fs::path& crop_dir, Length bleed_edge, const std::string& color_cube_name);
ImgDict RunCropper(const fs::path& image_dir,
                   const fs::path& crop_dir,
                   const fs::path& img_cache_file,
                   const ImgDict& img_dict,
                   Length bleed_edge,
                   PixelDensity max_density,
                   const std::string& color_cube_name,
                   const cv::Mat* color_cube,
                   bool uncrop,
                   PrintFn print_fn);

bool NeedRunMinimalCropper(const fs::path& image_dir,
                           const fs::path& crop_dir,
                           std::span<const fs::path> card_list,
                           Length bleed_edge,
                           const std::string& color_cube_name);
void RunMinimalCropper(const fs::path& image_dir,
                       const fs::path& crop_dir,
                       std::span<const fs::path> card_list,
                       Length bleed_edge,
                       PixelDensity max_density,
                       const std::string& color_cube_name,
                       const cv::Mat* color_cube,
                       PrintFn print_fn);

bool NeedCachePreviews(const fs::path& crop_dir, const ImgDict& img_dict);
ImgDict CachePreviews(const fs::path& image_dir, const fs::path& crop_dir, const fs::path& img_cache_file, const ImgDict& img_dict, PrintFn print_fn);

ImgDict ReadPreviews(const fs::path& img_cache_file);
void WritePreviews(const fs::path& img_cache_file, const ImgDict& img_dict);

cv::Mat LoadColorCube(const fs::path& file_path);
