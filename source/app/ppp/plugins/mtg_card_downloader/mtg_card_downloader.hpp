#pragma once

#include <ppp/plugins/plugin_interface.hpp>

PluginInterface* InitMtGCardDownloaderPlugin(Project& project, const Config& config);
void DestroyMtGCardDownloaderPlugin(PluginInterface* widget);
