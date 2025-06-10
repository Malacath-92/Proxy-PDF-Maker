#pragma once

#include <QScrollArea>

class QVBoxLayout;

class PrintProxyPrepApplication;
class Project;

class OptionsAreaWidget : public QScrollArea
{
    Q_OBJECT

  public:
    OptionsAreaWidget(const PrintProxyPrepApplication& app,
                      Project& project,
                      QWidget* actions,
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

    const PrintProxyPrepApplication& m_App;
    Project& m_Project;

    std::unordered_map<std::string_view, QWidget*> m_PluginWidgets;
};
