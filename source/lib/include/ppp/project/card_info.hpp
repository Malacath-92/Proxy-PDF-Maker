#pragma once

#include <ppp/util.hpp>
#include <ppp/image.hpp>

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

struct CardInfo
{
    fs::path m_Name{};

    uint32_t m_Num{ 1 };
    uint32_t m_Hidden{ 0 };

    fs::path m_Backside{};
    bool m_BacksideShortEdge{ false };
    bool m_BacksideAutoAssigned{ false };

    Image::Rotation m_Rotation{ Image::Rotation::None };
    BleedType m_BleedType{ BleedType ::Default };
    BadAspectRatioHandling m_BadAspectRatioHandling{ BadAspectRatioHandling::Default };

    fs::file_time_type m_LastWriteTime{};

    bool m_Transient{ false };
};
