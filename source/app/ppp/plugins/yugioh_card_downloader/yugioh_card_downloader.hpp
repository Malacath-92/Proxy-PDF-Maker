#pragma once

#include <ppp/plugins/plugin_interface.hpp>

PluginInterface* InitYuGiOhCardDownloaderPlugin(Project& project);
void DestroyYuGiOhCardDownloaderPlugin(PluginInterface* widget);
