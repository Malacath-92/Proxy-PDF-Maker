#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include <ppp/svg/util.hpp>
#include <ppp/units.hpp>
#include <ppp/util.hpp>

enum class CardOrder
{
    Alphabetical,
    Backside,
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

enum class ImageCompression
{
    Lossless,
    Lossy,
    AsIs,
};

enum class PageOrientation
{
    Portrait,
    Landscape
};

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

struct CardSizeRoundedRectInfo
{
    SizeInfo m_CardSize;
    LengthInfo m_CornerRadius;
};

struct CardSizeSvgInfo
{
    std::string m_SvgName;
    Svg m_Svg;
};

struct CardSizeInfo
{
    LengthInfo m_InputBleed;
    std::string m_Hint;
    float m_CardSizeScale;

    // Cards defined as a rounded rect
    std::optional<CardSizeRoundedRectInfo> m_RoundedRect;

    // Cards defined as an arbitrary shape
    std::optional<CardSizeSvgInfo> m_SvgInfo;
};

using PageSizes = std::map<std::string, SizeInfo>;
using CardSizes = std::map<std::string, CardSizeInfo>;

inline constexpr std::string_view g_FitSize{ "Fit" };
inline constexpr std::string_view g_BasePDFSize{ "Base Pdf" };
inline constexpr std::array g_SpecialPagerSizes{
    g_FitSize,
    g_BasePDFSize,
};

inline constexpr auto c_CardSizeNames{
    std::views::keys |
    std::views::transform([](const auto& s)
                          { return std::string_view{ s }; })
};
inline constexpr auto c_CardSizeHints{
    std::views::values |
    std::views::transform(&CardSizeInfo::m_Hint) |
    std::views::transform([](const auto& s)
                          { return std::string_view{ s }; })
};
inline constexpr auto c_PageSizeNames{
    std::views::keys |
    std::views::transform([](const auto& s)
                          { return std::string_view{ s }; })
};
