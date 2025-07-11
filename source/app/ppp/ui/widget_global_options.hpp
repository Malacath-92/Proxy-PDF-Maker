#pragma once

#include <QWidget>

class PrintProxyPrepApplication;
class Project;

class GlobalOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GlobalOptionsWidget(PrintProxyPrepApplication& application);

  signals:
    void BaseUnitChanged();
    void DisplayColumnsChanged();
    void RenderBackendChanged();
    void ImageFormatChanged();
    void JpgQualityChanged();
    void EnableUncropChanged();
    void ColorCubeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();

    void PluginEnabled(std::string_view plugin_name);
    void PluginDisabled(std::string_view plugin_name);
};
