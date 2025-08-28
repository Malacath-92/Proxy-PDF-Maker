#pragma once

#include <optional>
#include <string_view>

#include <ppp/util.hpp>

enum class Unit
{
    Millimeter,
    Centimeter,
    Inches,
    Points,
};

constexpr Length UnitValue(Unit unit);
constexpr std::string_view UnitName(Unit unit);
constexpr std::string_view UnitShortName(Unit unit);

constexpr std::optional<Unit> UnitFromName(std::string_view unit_name);
constexpr std::optional<Unit> UnitFromValue(Length unit_value);

#include <ppp/units.inl>
