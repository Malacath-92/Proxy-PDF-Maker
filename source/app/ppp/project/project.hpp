#pragma once

#include <cstdint>
#include <unordered_map>

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
    Image CroppedImage;
    Image UncroppedImage;
    Image CroppedThumbImage;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

struct Project
{
    void Load(const fs::path& json_path, PrintFn print_fn);
    void Dump(const fs::path& json_path, PrintFn print_fn) const;

    void Init(PrintFn print_fn);
    void InitProperties(PrintFn print_fn);
    void InitImages(PrintFn print_fn);

    // Project options
    fs::path ImageDir{ "images" };
    fs::path CropDir{ "images/crop" };
    fs::path ImageCache{ "img.cache" };

    // List of all cards
    CardMap Cards{};
    ImgDict Previews{};

    // Bleed edge options
    Length BleedEdge{ 0_mm };

    // Backside options
    bool BacksideEnabled{ false };
    fs::path BacksideDefault{ "__back.png" };
    Length BacksideOffset{ 0_mm };

    // Oversized options
    bool OversizedEnabled{ false };

    // PDF generation options
    std::string PageSize{ CFG.DefaultPageSize };
    std::string Orientation{ "Portrait" };
    fs::path FileName{ "_printme" };
    bool ExtendedGuides{ false };
};