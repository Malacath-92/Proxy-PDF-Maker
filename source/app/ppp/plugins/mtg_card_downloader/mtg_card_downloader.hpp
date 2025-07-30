#pragma once

#include <ppp/plugins/plugin_interface.hpp>

PluginInterface* InitMtGCardDownloaderPlugin(Project& project);
void DestroyMtGCardDownloaderPlugin(PluginInterface* widget);
