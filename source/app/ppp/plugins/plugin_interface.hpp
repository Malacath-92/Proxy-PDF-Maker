#pragma once

#include <QObject>

#include <ppp/util.hpp>

class QWidget;

class Project;

class PluginInterface : public QObject
{
    Q_OBJECT

  public:
    virtual QWidget* Widget() = 0;

    void Route(PluginInterface& other)
    {
#define ROUTE(method)                          \
    QObject::connect(&other,                   \
                     &PluginInterface::method, \
                     this,                     \
                     &PluginInterface::method)
        ROUTE(PauseCropper);
        ROUTE(UnpauseCropper);
        ROUTE(RefreshCardGrid);
#undef ROUTE
    }

  signals:
    void PauseCropper();
    void UnpauseCropper();
    void RefreshCardGrid();
};

using PluginInit = PluginInterface*(Project& project);
using PluginDestroy = void(PluginInterface* plugin_widget);

struct Plugin
{
    std::string_view m_Name{ "Plugin Name" };
    PluginInit* m_Init{ nullptr };
    PluginDestroy* m_Destroy{ nullptr };
};
