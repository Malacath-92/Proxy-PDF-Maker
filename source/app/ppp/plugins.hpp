#pragma once

#include <string>
#include <string_view>
#include <vector>

class QWidget;
class Project;

std::vector<std::string_view> GetPluginNames();
QWidget* InitPlugin(std::string_view plugin_name, Project& project);
void DestroyPlugin(std::string_view plugin_name, QWidget* plugin_widget);
