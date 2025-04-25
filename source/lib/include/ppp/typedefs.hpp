#pragma once

#include <cstdint>

#include <ppp/util/bit_field.hpp>

enum class LogFlags : uint32_t
{
    Console = Bit(1u),
    File = Bit(2u),
    FatalQuit = Bit(3u),
    DetailTime = Bit(4u),
    DetailFile = Bit(5u),
    DetailLine = Bit(6u),
    DetailColumn = DetailLine | Bit(7u),
    DetailFunction = Bit(8u),
    DetailThread = Bit(9u),
    DetailErrorStacktrace = Bit(10u),
    DetailFatalStacktrace = Bit(11u),

    DetailStacktrace = DetailErrorStacktrace | DetailFatalStacktrace,
    DetailAll = DetailTime | DetailFile | DetailLine | DetailColumn | DetailFunction | DetailThread,
    DetailAllStacktrace = DetailAll | DetailStacktrace,
};
ENABLE_BITFIELD_OPERATORS(LogFlags);
