#pragma once

#include <cstdint>
#include <unordered_map>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/constants.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

struct CardInfo
{
    uint32_t Num{ 1 };
    fs::path Backside{};
    bool BacksideShortEdge{ false };
    bool Oversized{ false };
};
using CardMap = std::unordered_map<fs::path, CardInfo>;

struct ImagePreview
{
    Image UncroppedImage;
    Image CroppedImage;
    Image CroppedThumbImage;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

struct Project
{
    Project() = default;

    void Load(const fs::path& json_path, const ColorCube* color_cube, PrintFn print_fn);
    void Dump(const fs::path& json_path, PrintFn print_fn) const;

    void Init(const ColorCube* color_cube, PrintFn print_fn);
    void InitProperties(PrintFn print_fn);
    void InitImages(const ColorCube* color_cube, PrintFn print_fn);

    const ImagePreview& GetPreview(const fs::path& image_name) const;
    const ImagePreview& GetBacksidePreview(const fs::path& image_name) const;

    const fs::path& GetBacksideImage(const fs::path& image_name) const;

    // Project options
    fs::path ImageDir{ "images" };
    fs::path CropDir{ "images/crop" };
    fs::path ImageCache{ "proj.cache" };

    // List of all cards
    CardMap Cards{};
    ImgDict Previews{};
    ImagePreview FallbackPreview{};

    // Bleed edge options
    Length BleedEdge{ 0_mm };
    float CornerWeight{ 1.0f };

    // Backside options
    bool BacksideEnabled{ false };
    bool BacksideEnableGuides{ false };
    fs::path BacksideDefault{ "__back.png" };
    Length BacksideOffset{ 0_mm };

    // Oversized options
    bool OversizedEnabled{ false };

    // PDF generation options
    std::string PageSize{ CFG.DefaultPageSize };
    std::string Orientation{ "Portrait" };
    fs::path FileName{ "_printme" };
    bool EnableGuides{ true };
    bool ExtendedGuides{ false };
    ColorRGB8 GuidesColorA{ 0, 0, 0 };
    ColorRGB8 GuidesColorB{ 190, 190, 190 };

  private:
    Project(const Project&) = default;
    Project(Project&&) = default;
    Project& operator=(const Project&) = default;
    Project& operator=(Project&&) = default;
};
