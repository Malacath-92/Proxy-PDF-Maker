#pragma once

#include <any>

#include <QObject>

#include <ppp/util.hpp>
#include <ppp/util/bit_field.hpp>
#include <ppp/util/log.hpp>

class QWidget;

class Project;
class Config;

enum class PluginWidgetExtensionType
{
    CardWidgetExtension,
};

class PluginInterface : public QObject
{
    Q_OBJECT

  public:
    virtual QWidget* Widget() = 0;

    virtual bool ProvidesWidgetExtension(PluginWidgetExtensionType /* type */) const
    { return false; }
    virtual QWidget* MakeCardWidgetExtension(const fs::path& /* card_name */)
    { return nullptr; }

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
        ROUTE(SetCardSizeChoice);
        ROUTE(SetEnableBackside);
        ROUTE(SetBacksideAutoPattern);
#undef ROUTE
    }

  signals:
    void PauseCropper();
    void UnpauseCropper();
    void RefreshCardGrid();

    void SetCardSizeChoice(const std::string& card_size_choice);
    void SetEnableBackside(bool enabled);

    void SetBacksideAutoPattern(const std::string& pattern);
};

using PluginInit = PluginInterface*(Project& project, const Config& config);
using PluginDestroy = void(PluginInterface* plugin_widget);

struct Plugin
{
    std::string_view m_Name{ "Plugin Name" };
    PluginInit* m_Init{ nullptr };
    PluginDestroy* m_Destroy{ nullptr };
};
