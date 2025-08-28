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

enum class Unit
{
    Millimeter,
    Centimeter,
    Inches,
    Points,
};

struct UnitInfo
{
    std::string_view m_Name;
    std::string_view m_ShortName;
    Length m_Unit;
    Unit m_Type;

    // clang-format off
    constexpr auto GetName() const { return m_Name; }
    constexpr auto GetShortName() const { return m_ShortName; }
    constexpr auto GetUnit() const { return m_Unit; }
    // clang-format on
};

struct Config
{
    bool m_EnableFancyUncrop{ true };
    Pixel m_BasePreviewWidth{ 248_pix };
    PixelDensity m_MaxDPI{ 1200_dpi };
    uint32_t m_DisplayColumns{ 5 };
    std::string m_DefaultCardSize{ "Standard" };
    std::string m_DefaultPageSize{ "Letter" };
    std::string m_ColorCube{ "None" };
    fs::path m_FallbackName{ "fallback.png"_p };
    PdfBackend m_Backend{ PdfBackend::LibHaru };
    ImageFormat m_PdfImageFormat{ ImageFormat::Png };
    std::optional<int> m_PngCompression{ std::nullopt };
    std::optional<int> m_JpgQuality{ std::nullopt };
    UnitInfo m_BaseUnit{ c_SupportedBaseUnits[0] };

    std::unordered_map<std::string, bool> m_PluginsState;

    static inline constexpr std::string_view c_FitSize{ "Fit" };
    static inline constexpr std::string_view c_BasePDFSize{ "Base Pdf" };

    struct SizeInfo
    {
        Size m_Dimensions;
        Length m_BaseUnit;
        uint32_t m_Decimals;
    };
    struct LengthInfo
    {
        Length m_Dimension;
        Length m_BaseUnit;
        uint32_t m_Decimals;
    };

    struct CardSizeInfo
    {
        SizeInfo m_CardSize;
        LengthInfo m_InputBleed;
        LengthInfo m_CornerRadius;
        std::string m_Hint;
        float m_CardSizeScale{ 1.0f };
    };

    std::map<std::string, SizeInfo> m_DefaultPageSizes{
        { "Letter", { { 8.5_in, 11_in }, 1_in, 1u } },
        { "Legal", { { 8.5_in, 14_in }, 1_in, 1u } },
        { "Ledger", { { 11_in, 17_in }, 1_in, 1u } },
        { "A5", { { 148.5_mm, 210_mm }, 1_mm, 1u } },
        { "A4", { { 210_mm, 297_mm }, 1_mm, 0u } },
        { "A4+", { { 240_mm, 329_mm }, 1_mm, 0u } },
        { "A3", { { 297_mm, 420_mm }, 1_mm, 0u } },
        { "A3+", { { 329_mm, 483_mm }, 1_mm, 0u } },
        { std::string{ c_FitSize }, {} },
        { std::string{ c_BasePDFSize }, {} },
    };
    std::map<std::string, SizeInfo> m_PageSizes{ m_DefaultPageSizes };

    std::map<std::string, CardSizeInfo> m_CardSizes{
        {
            "Standard",
            {
                .m_CardSize{ { 2.48_in, 3.46_in }, 1_in, 2u },
                .m_InputBleed{ 0.12_in, 1_in, 2u },
                .m_CornerRadius{ 2.5_mm, 1_mm, 1u },
                .m_Hint{ ".e.g. Magic the Gathering, Pokemon, and other TCGs" },
            },
        },
        {
            "Standard x2",
            {
                .m_CardSize{ { 3.46_in, 4.96_in }, 1_in, 2u },
                .m_InputBleed{ 0.12_in, 1_in, 2u },
                .m_CornerRadius{ 5_mm, 1_mm, 1u },
                .m_Hint{ ".e.g. oversized Magic the Gathering" },
            },
        },
        {
            "Standard Novelty",
            {
                .m_CardSize{ { 2.48_in, 3.46_in }, 1_in, 2u },
                .m_InputBleed{ 0.12_in, 1_in, 2u },
                .m_CornerRadius{ 2.5_mm, 1_mm, 1u },
                .m_Hint{ ".e.g. novelty-sized Magic the Gathering" },
                .m_CardSizeScale = 0.5f,
            },
        },
        {
            "Japanese",
            {
                .m_CardSize{ { 59_mm, 86_mm }, 1_mm, 0u },
                .m_InputBleed{ 2_mm, 1_mm, 0u },
                .m_CornerRadius{ 1_mm, 1_mm, 0u },
                .m_Hint{ ".e.g. Yu-Gi-Oh!" },
            },
        },
        {
            "Poker",
            {
                .m_CardSize{ { 2.5_in, 3.5_mm }, 1_in, 1u },
                .m_InputBleed{ 2_mm, 1_mm, 0u },
                .m_CornerRadius{ 3_cm, 1_mm, 0u },
                .m_Hint{},
            },
        },
    };

    void SetPdfBackend(PdfBackend backend);

    std::string_view GetFirstValidPageSize() const;

    static inline constexpr std::array c_SupportedBaseUnits{
        UnitInfo{
            "mm",
            "mm",
            1_mm,
            Unit::Millimeter,
        },
        UnitInfo{
            "cm",
            "cm",
            10_mm,
            Unit::Centimeter,
        },
        UnitInfo{
            "inches",
            "in",
            1_in,
            Unit::Inches,
        },
        UnitInfo{
            "points",
            "pts",
            1_pts,
            Unit::Points,
        },
    };

    static inline constexpr std::optional<UnitInfo> GetUnit(Unit unit_type)
    {
        for (const auto& unit_info : c_SupportedBaseUnits)
        {
            if (unit_info.m_Type == unit_type)
            {
                return unit_info;
            }
        }
        return std::nullopt;
    }
    static inline constexpr std::optional<UnitInfo> GetUnitFromName(std::string_view unit_name)
    {
        for (const auto& unit_info : c_SupportedBaseUnits)
        {
            if (unit_info.m_Name == unit_name)
            {
                return unit_info;
            }
        }
        return std::nullopt;
    }
    static inline constexpr std::optional<UnitInfo> GetUnitFromValue(Length unit_value)
    {
        for (const auto& unit_info : c_SupportedBaseUnits)
        {
            if (dla::math::abs(unit_info.m_Unit - unit_value) < 0.0001_mm)
            {
                return unit_info;
            }
        }
        return std::nullopt;
    }
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config g_Cfg;
