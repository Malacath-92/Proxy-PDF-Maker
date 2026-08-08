#pragma once

#include <string>
#include <string_view>
#include <vector>

class Project;
class Config;
class PluginInterface;

std::vector<std::string_view> GetPluginNames();
PluginInterface* InitPlugin(std::string_view plugin_name,
                            Project& project,
                            const Config& config);
void DestroyPlugin(std::string_view plugin_name, PluginInterface* plugin);
