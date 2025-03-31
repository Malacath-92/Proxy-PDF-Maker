#pragma once

#include <QMainWindow>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_options.hpp>
#include <ppp/ui/widget_tabs.hpp>

class PrintProxyPrepMainWindow : public QMainWindow
{
    Q_OBJECT;

  public:
    PrintProxyPrepMainWindow(MainTabs* tabs, OptionsWidget* options);

    void OpenAboutPopup();

  signals:
    void NewProjectOpened(Project& project);
    void ImageDirChanged(Project& project);
    void PageSizeChanged(Project& project);
    void CardLayoutChanged(Project& project);
    void OrientationChanged(Project& project);
    void GuidesEnabledChanged(Project& project);
    void GuidesColorChanged(Project& project);
    void BleedChanged(Project& project);
    void CornerWeightChanged(Project& project);
    void BacksideEnabledChanged(Project& project);
    void BacksideGuidesEnabledChanged(Project& project);
    void BacksideDefaultChanged(Project& project);
    void BacksideOffsetChanged(Project& project);
    void OversizedEnabledChanged(Project& project);
    void DisplayColumnsChanged(Project& project);
    void EnableUncropChanged(Project& project);
    void ColorCubeChanged(Project& project);
    void BasePreviewWidthChanged(Project& project);
    void MaxDPIChanged(Project& project);

    // These exist solely to get the data safely to another thread
    void NewProjectOpenedDiff(const Project::ProjectData& data);
    void ImageDirChangedDiff(const fs::path& image_dir, const fs::path& crop_dir);
    void BleedChangedDiff(Length bleed);
    void EnableUncropChangedDiff(bool enable_uncrop);
    void ColorCubeChangedDiff(const std::string& cube_name);
    void BasePreviewWidthChangedDiff(Pixel base_preview_width);
    void MaxDPIChangedDiff(PixelDensity dpi);
};
