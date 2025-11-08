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

struct CardInfo
{
    fs::path m_Name{};
    std::optional<fs::path> m_ExternalPath{ std::nullopt };

    uint32_t m_Num{ 1 };
    uint32_t m_Hidden{ 0 };

    fs::path m_Backside{};
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
