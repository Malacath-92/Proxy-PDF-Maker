#pragma once

class QWidget;

class Project;

using PluginInit = QWidget*(Project& project);
using PluginDestroy = void(QWidget* plugin_widget);

struct Plugin
{
    std::string_view m_Name{ "Plugin Name" };
    PluginInit* m_Init{ nullptr };
    PluginDestroy* m_Destroy{ nullptr };
};
