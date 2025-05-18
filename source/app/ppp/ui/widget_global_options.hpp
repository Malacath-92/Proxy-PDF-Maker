#pragma once

#include <QWidget>

class PrintProxyPrepApplication;
class Project;

class GlobalOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GlobalOptionsWidget(PrintProxyPrepApplication& application, Project& project);

  signals:
    void BaseUnitChanged();
    void DisplayColumnsChanged();
    void RenderBackendChanged();
    void EnableUncropChanged();
    void ColorCubeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();
};
