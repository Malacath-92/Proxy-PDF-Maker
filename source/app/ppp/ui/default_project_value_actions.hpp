#pragma once

#include <functional>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

class QWidget;

struct Config;

void EnableOptionWidgetForDefaults(
    QWidget* widget,
    const Config& config,
    std::string_view path,
    std::function<void(nlohmann::json)> set_value = nullptr,
    std::function<nlohmann::json()> get_value = nullptr);
void ResetToDefault(
    QWidget* widget);