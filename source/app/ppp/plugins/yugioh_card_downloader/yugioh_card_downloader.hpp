#pragma once

#include <ppp/plugins/plugin_interface.hpp>

PluginInterface* InitYuGiOhCardDownloaderPlugin(Project& project, const Config& config);
void DestroyYuGiOhCardDownloaderPlugin(PluginInterface* widget);
