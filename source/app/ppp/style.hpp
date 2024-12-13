#pragma once

#include <array>
#include <string_view>

class QApplication;

using namespace std::string_view_literals;
inline constexpr std::array AvailableStyles{
    "Default"sv,
    "Fusion"sv,
    "Combinear"sv,
    "Darkeum"sv,
    "Wstartpage"sv,
};

void SetStyle(QApplication& application, std::string_view style);
