#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include <ppp/util.hpp>

enum class PdfBackend
{
    LibHaru,
    PoDoFo,
    Png,
};

enum class ImageFormat
{
    Png,
    Jpg
};

enum class PageOrientation
{
    Portrait,
    Landscape
};

struct Config
{
    bool EnableUncrop{ false };
    bool EnableFancyUncrop{ true };
    Pixel BasePreviewWidth{ 248_pix };
    PixelDensity MaxDPI{ 1200_dpi };
    uint32_t DisplayColumns{ 5 };
    std::string DefaultCardSize{ "Magic the Gathering" };
    std::string DefaultPageSize{ "Letter" };
    std::string ColorCube{ "None" };
    fs::path FallbackName{ "fallback.png"_p };
    PdfBackend Backend{ PdfBackend::LibHaru };
    ImageFormat PdfImageFormat{ ImageFormat::Png };
    std::optional<int> PngCompression{ std::nullopt };
    std::optional<int> JpgQuality{ std::nullopt };
    Length BaseUnit{ 1_mm };

    static inline constexpr std::array SupportedBaseUnits{
        std::string_view{ "inches" },
        std::string_view{ "points" },
        std::string_view{ "mm" },
        std::string_view{ "cm" },
    };

    static inline constexpr std::string_view FitSize{ "Fit" };
    static inline constexpr std::string_view BasePDFSize{ "Base Pdf" };

    struct SizeInfo
    {
        Size Dimensions;
        Length BaseUnit;
        uint32_t Decimals;
    };
    struct LengthInfo
    {
        Length Dimension;
        Length BaseUnit;
        uint32_t Decimals;
    };

    struct CardSizeInfo
    {
        SizeInfo CardSize;
        LengthInfo InputBleed;
        LengthInfo CornerRadius;
        float CardSizeScale{ 1.0f };
    };

    std::map<std::string, SizeInfo> PageSizes{
        { "Letter", { { 8.5_in, 11_in }, 1_in, 1u } },
        { "Legal", { { 14_in, 8.5_in }, 1_in, 1u } },
        { "A5", { { 148.5_mm, 210_mm }, 1_mm, 1u } },
        { "A4", { { 210_mm, 297_mm }, 1_mm, 0u } },
        { "A3", { { 297_mm, 420_mm }, 1_mm, 0u } },
        { std::string{ FitSize }, {} },
        { std::string{ BasePDFSize }, {} },
    };

    std::map<std::string, CardSizeInfo> CardSizes{
        {
            "Magic the Gathering",
            {
                .CardSize{ { 2.48_in, 3.46_in }, 1_in, 2u },
                .InputBleed{ 0.12_in, 1_in, 2u },
                .CornerRadius{ 1_in / 8, 1_in, 3u },
            },
        },
        {
            "MtG Oversized",
            {
                .CardSize{ { 3.46_in, 4.96_in }, 1_in, 2u },
                .InputBleed{ 0.12_in, 1_in, 2u },
                .CornerRadius{ 1_in / 4, 1_in, 2u },
            },
        },
        {
            "MtG Novelty",
            {
                .CardSize{ { 3.46_in, 4.96_in }, 1_in, 2u },
                .InputBleed{ 0.12_in, 1_in, 2u },
                .CornerRadius{ 1_in / 4, 1_in, 2u },
                .CardSizeScale = 0.5f,
            },
        },
        {
            "Yu-Gi-Oh",
            {
                .CardSize{ { 59_mm, 86_mm }, 1_mm, 0u },
                .InputBleed{ 2_mm, 1_mm, 0u },
                .CornerRadius{ 1_mm, 1_mm, 0u },
            },
        },
    };

    void SetPdfBackend(PdfBackend backend);
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config CFG;
