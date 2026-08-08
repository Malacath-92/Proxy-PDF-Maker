#pragma once

#include <QScrollArea>

class QVBoxLayout;

class Project;
class Config;
class PluginInterface;

class OptionsAreaWidget : public QScrollArea
{
    Q_OBJECT

  public:
    OptionsAreaWidget(
        Project& project,
        const Config& config,
        PluginInterface& plugin_router,
        QWidget* project_options,
        QWidget* print_options,
        QWidget* guides_options,
        QWidget* card_options,
        QWidget* global_options);

  public slots:
    void PluginEnabled(std::string_view plugin_name);
    void PluginDisabled(std::string_view plugin_name);

  signals:
    void SetObjectVisibility(QString object_name, bool visible);

  private:
    void AddCollapsible(QVBoxLayout* layout, QWidget* widget);

    Project& m_Project;
    const Config& m_Cfg;

    PluginInterface& m_PluginRouter;
    std::unordered_map<std::string_view, PluginInterface*> m_Plugins;
};
