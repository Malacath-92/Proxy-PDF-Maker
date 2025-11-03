#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include <ppp/units.hpp>
#include <ppp/util.hpp>

enum class CardOrder
{
    Alphabetical,
    LastModified,
    LastAdded,
};

enum class CardOrderDirection
{
    Ascending,
    Descending,
};

enum class PdfBackend
{
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
    bool m_AdvancedMode{ false };

    bool m_CheckVersionOnStartup{ true };

    bool m_EnableFancyUncrop{ true };
    Pixel m_BasePreviewWidth{ 248_pix };
    PixelDensity m_MaxDPI{ 1200_dpi };
    CardOrder m_CardOrder{ CardOrder::Alphabetical };
    CardOrderDirection m_CardOrderDirection{ CardOrderDirection::Ascending };
    uint32_t m_MaxWorkerThreads{ 16 };
    uint32_t m_DisplayColumns{ 5 };
    std::string m_DefaultCardSize{ "Standard" };
    std::string m_DefaultPageSize{ "Letter" };
    std::string m_ColorCube{ "None" };
    fs::path m_FallbackName{ "fallback.png"_p };
    PdfBackend m_Backend{ PdfBackend::PoDoFo };
    ImageFormat m_PdfImageFormat{ ImageFormat::Jpg };
    std::optional<int> m_PngCompression{ std::nullopt };
    std::optional<int> m_JpgQuality{ std::nullopt };
    Unit m_BaseUnit{ Unit::Inches };

    std::unordered_map<std::string, bool> m_PluginsState{};

    static inline constexpr std::string_view c_FitSize{ "Fit" };
    static inline constexpr std::string_view c_BasePDFSize{ "Base Pdf" };

    struct SizeInfo
    {
        Size m_Dimensions;
        Unit m_BaseUnit;
        uint32_t m_Decimals;
    };
    struct LengthInfo
    {
        Length m_Dimension;
        Unit m_BaseUnit;
        uint32_t m_Decimals;
    };

    struct CardSizeInfo
    {
        SizeInfo m_CardSize;
        LengthInfo m_InputBleed;
        LengthInfo m_CornerRadius;
        std::string m_Hint;
        float m_CardSizeScale;
    };

    inline static const std::map<std::string, SizeInfo> g_DefaultPageSizes{
        { "Letter", { { 8.5_in, 11_in }, Unit::Inches, 1u } },
        { "Legal", { { 8.5_in, 14_in }, Unit::Inches, 1u } },
        { "Ledger", { { 11_in, 17_in }, Unit::Inches, 1u } },
        { "A5", { { 148.5_mm, 210_mm }, Unit::Millimeter, 1u } },
        { "A4", { { 210_mm, 297_mm }, Unit::Millimeter, 0u } },
        { "A4+", { { 240_mm, 329_mm }, Unit::Millimeter, 0u } },
        { "A3", { { 297_mm, 420_mm }, Unit::Millimeter, 0u } },
        { "A3+", { { 329_mm, 483_mm }, Unit::Millimeter, 0u } },
        { std::string{ c_FitSize }, {} },
        { std::string{ c_BasePDFSize }, {} },
    };
    std::map<std::string, SizeInfo> m_PageSizes{ g_DefaultPageSizes };

    inline static const std::map<std::string, CardSizeInfo> g_DefaultCardSizes{
        {
            "Standard",
            {
                .m_CardSize{ { 2.48_in, 3.46_in }, Unit::Inches, 2u },
                .m_InputBleed{ 0.12_in, Unit::Inches, 2u },
                .m_CornerRadius{ 2.5_mm, Unit::Millimeter, 1u },
                .m_Hint{ ".e.g. Magic the Gathering, Pokemon, and other TCGs" },
                .m_CardSizeScale = 1.0f,
            },
        },
        {
            "Oversized",
            {
                .m_CardSize{ { 3.46_in, 4.96_in }, Unit::Inches, 2u },
                .m_InputBleed{ 0.12_in, Unit::Inches, 2u },
                .m_CornerRadius{ 5_mm, Unit::Millimeter, 1u },
                .m_Hint{ ".e.g. oversized Magic the Gathering" },
                .m_CardSizeScale = 1.0f,
            },
        },
        {
            "Novelty",
            {
                .m_CardSize{ { 2.48_in, 3.46_in }, Unit::Inches, 2u },
                .m_InputBleed{ 0.12_in, Unit::Inches, 2u },
                .m_CornerRadius{ 2.5_mm, Unit::Millimeter, 1u },
                .m_Hint{ ".e.g. novelty-sized Magic the Gathering" },
                .m_CardSizeScale = 0.5f,
            },
        },
        {
            "Japanese",
            {
                .m_CardSize{ { 59_mm, 86_mm }, Unit::Millimeter, 0u },
                .m_InputBleed{ 2_mm, Unit::Millimeter, 0u },
                .m_CornerRadius{ 1_mm, Unit::Millimeter, 0u },
                .m_Hint{ ".e.g. Yu-Gi-Oh!" },
                .m_CardSizeScale = 1.0f,
            },
        },
        {
            "Poker",
            {
                .m_CardSize{ { 2.5_in, 3.5_in }, Unit::Inches, 1u },
                .m_InputBleed{ 3_mm, Unit::Millimeter, 0u },
                .m_CornerRadius{ 3_mm, Unit::Millimeter, 0u },
                .m_Hint{},
                .m_CardSizeScale = 1.0f,
            },
        },
    };
    std::map<std::string, CardSizeInfo> m_CardSizes{ g_DefaultCardSizes };

    void SetPdfBackend(PdfBackend backend);

    std::string_view GetFirstValidPageSize() const;
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config g_Cfg;
