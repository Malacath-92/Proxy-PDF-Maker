#include <ppp/units.hpp>

#include <utility>

#include <magic_enum/magic_enum.hpp>

constexpr Length UnitValue(Unit unit)
{
    switch (unit)
    {
    case Unit::Millimeter:
        return 1_mm;
    case Unit::Centimeter:
        return 1_cm;
    case Unit::Inches:
        return 1_in;
    case Unit::Points:
        return 1_pts;
    }

    std::unreachable();
}
constexpr std::string_view UnitName(Unit unit)
{
    switch (unit)
    {
    case Unit::Millimeter:
        return "mm";
    case Unit::Centimeter:
        return "cm";
    case Unit::Inches:
        return "inches";
    case Unit::Points:
        return "points";
    }

    std::unreachable();
}
constexpr std::string_view UnitShortName(Unit unit)
{
    switch (unit)
    {
    case Unit::Millimeter:
        return "mm";
    case Unit::Centimeter:
        return "cm";
    case Unit::Inches:
        return "in";
    case Unit::Points:
        return "pts";
    }

    std::unreachable();
}

constexpr std::optional<Unit> UnitFromName(std::string_view unit_name)
{
    for (const auto& unit : magic_enum::enum_values<Unit>())
    {
        if (UnitName(unit) == unit_name)
        {
            return unit;
        }
    }
    return std::nullopt;
}
constexpr std::optional<Unit> UnitFromValue(Length unit_value)
{
    for (const auto& unit : magic_enum::enum_values<Unit>())
    {
        if (dla::math::abs(UnitValue(unit) - unit_value) < 0.0001_mm)
        {
            return unit;
        }
    }
    return std::nullopt;
}
