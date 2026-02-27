#pragma once

#include <chrono>
#include <optional>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

struct ProjectData;

enum class BadAspectRatioHandling
{
    Ignore,
    Expand,
    Stretch,

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

struct CardInfo
{
    fs::path m_Name{};
    std::optional<fs::path> m_ExternalPath{ std::nullopt };

    uint32_t m_Num{ 1 };
    uint32_t m_Hidden{ 0 };

    std::optional<fs::path> m_Backside{ ""_p };
    bool m_BacksideShortEdge{ false };
    bool m_BacksideAutoAssigned{ false };

    Image::Rotation m_Rotation{ Image::Rotation::None };
    BleedType m_BleedType{ BleedType ::Default };
    BadAspectRatioHandling m_BadAspectRatioHandling{ BadAspectRatioHandling::Default };

    fs::file_time_type m_LastWriteTime{};
    CardInfoTimePoint m_TimeAdded{};

    bool m_Transient{ false };

    fs::path GetSourcePath(const ProjectData& data) const;
    fs::path GetSourceFolder(const ProjectData& data) const;
};

inline bool HasDefaultBackside(const CardInfo& card)
{
    return card.m_Backside.has_value() && card.m_Backside.value().empty();
}
inline bool HasNonClearNonDefaultBackside(const CardInfo& card)
{
    return card.m_Backside.has_value() && !card.m_Backside.value().empty();
}
