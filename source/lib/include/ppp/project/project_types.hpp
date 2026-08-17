#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

enum class BadAspectRatioHandling
{
    Ignore,
    Expand,
    Stretch,
    Crop,

    Default = Ignore,
};

enum class BleedType
{
    Infer,
    FullBleed,
    NoBleed,

    Default = Infer,
};

using CardInfoClock = std::chrono::high_resolution_clock;
using CardInfoTimePoint = CardInfoClock::time_point;
using OptionalImageRef = std::optional<std::reference_wrapper<const fs::path>>;

using CardContainer = std::vector<struct CardInfo>;
using CardSorting = std::vector<fs::path>;

struct ImagePreview
{
    Image m_UncroppedImage;
    Image m_CroppedImage;
    bool m_BadAspectRatio;
    bool m_BadRotation;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

enum class FlipPageOn
{
    LeftEdge,
    TopEdge,
};

enum class CardCorners
{
    Square,
    Rounded,
};

enum class MarginsMode
{
    Auto,
    Simple,
    Full,
    Linked,
};

enum class Margin
{
    Left,
    Top,
    Right,
    Bottom,
    All,
};

template<class T>
struct GenericMargins
{
    T m_Left{};
    T m_Top{};
    T m_Right{};
    T m_Bottom{};

    template<class U>
    auto operator*(const U& rhs)
    {
        using ResT = decltype(std::declval<T>() * std::declval<U>());
        return GenericMargins<ResT>{
            m_Left * rhs,
            m_Top * rhs,
            m_Right * rhs,
            m_Bottom * rhs,
        };
    }
    template<class U>
    auto operator/(const U& rhs)
    {
        using ResT = decltype(std::declval<T>() / std::declval<U>());
        return GenericMargins<ResT>{
            m_Left / rhs,
            m_Top / rhs,
            m_Right / rhs,
            m_Bottom / rhs,
        };
    }
};
using Margins = GenericMargins<Length>;

// Individual margin controls allow for asymmetric layouts needed in professional printing
// where different margins are required for binding, cutting, or aesthetic purposes
struct CustomMargins
{
    Size m_TopLeft{ 0_mm, 0_mm };
    std::optional<Size> m_BottomRight{ std::nullopt };
};

enum CardOrientation
{
    Vertical,
    Horizontal,
    Mixed,
};
