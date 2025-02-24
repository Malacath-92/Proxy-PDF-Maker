#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <ppp/util.hpp>

enum class PdfBackend
{
    LibHaru,
    Hummus,
};

struct Config
{
    bool EnableUncrop{ false };
    Pixel BasePreviewWidth{ 248_pix };
    PixelDensity MaxDPI{ 1200_dpi };
    uint32_t DisplayColumns{ 5 };
    std::string DefaultPageSize{ "Letter" };
    std::string ColorCube{ "None" };
    PdfBackend Backend{ PdfBackend::LibHaru };

    struct PageSizeInfo
    {
        Size Dimensions;
        Length BaseUnit;
        uint32_t Decimals;
    };
    std::map<std::string, PageSizeInfo> PageSizes{
        { "Letter", { { 8.5_in, 11_in }, 1_in, 1u } },
        { "Legal", { { 14_in, 8.5_in }, 1_in, 1u } },
        { "A5", { { 148.5_mm, 210_mm }, 1_mm, 1u } },
        { "A4", { { 210_mm, 297_mm }, 1_mm, 0u } },
        { "A3", { { 297_mm, 420_mm }, 1_mm, 0u } },
    };
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config CFG;
