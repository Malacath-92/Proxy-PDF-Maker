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

inline const std::array g_ValidImageExtensions{
    ".bmp"_p,
    ".gif"_p,
    ".jpg"_p,
    ".jpeg"_p,
    ".png"_p,
    ".tif"_p,
    ".tiff"_p,
    ".webp"_p
};

std::vector<fs::path> ListImageFiles(const fs::path& path);
std::vector<fs::path> ListImageFiles(const fs::path& path_one, const fs::path& path_two);

Image CropImage(const Image& image,
                const fs::path& card_name,
                Size card_size,
                Length full_bleed,
                Length bleed_edge,
                PixelDensity max_density);
Image UncropImage(const Image& image, const fs::path& card_name, Size card_size, bool fancy_uncrop);

fs::path GetOutputDir(const fs::path& crop_dir, Length bleed_edge, const std::string& color_cube_name);

ImgDict ReadPreviews(const fs::path& img_cache_file);
void WritePreviews(const fs::path& img_cache_file, const ImgDict& img_dict);

cv::Mat LoadColorCube(const fs::path& file_path);
