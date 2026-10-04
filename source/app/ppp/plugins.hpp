#pragma once

#include <string>
#include <string_view>
#include <vector>

class Project;
class Config;
class PluginInterface;

std::span<const std::string_view> GetPluginNames();
PluginInterface* InitPlugin(std::string_view plugin_name,
                            Project& project,
                            const Config& config);
void DestroyPlugin(std::string_view plugin_name, PluginInterface* plugin);

PluginInterface* GetPlugin(std::string_view plugin_name);
