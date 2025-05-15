#pragma once

#include <QMainWindow>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_tabs.hpp>

class PrintProxyPrepMainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options);

    void OpenAboutPopup();

    virtual void closeEvent(QCloseEvent* event) override;

  signals:
    void NewProjectOpened(Project& project);
    void ImageDirChanged(Project& project);
    void BleedChanged(Project& project);
    void CornerWeightChanged(Project& project);
    void BacksideEnabledChanged(Project& project);
    void BacksideDefaultChanged(Project& project);
    void BacksideOffsetChanged(Project& project);
    void BaseUnitChanged(Project& project);
    void DisplayColumnsChanged(Project& project);
    void RenderBackendChanged(Project& project);
    void EnableUncropChanged(Project& project);
    void ColorCubeChanged(Project& project);
    void BasePreviewWidthChanged(Project& project);
    void MaxDPIChanged(Project& project);
};
