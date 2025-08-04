#pragma once

#include <QObject>

#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

// This exists purely to get data safely from the main thread to the cropper thread
class CropperThreadRouter : public QObject
{
    Q_OBJECT

  public:
    CropperThreadRouter(const Project& project);

  signals:
    void NewProjectOpenedDiff(const Project::ProjectData& data);
    void ImageDirChangedDiff(const fs::path& image_dir, const fs::path& crop_dir, const std::vector<fs::path>& loaded_previews);
    void CardSizeChangedDiff(std::string card_size);
    void BleedChangedDiff(Length bleed);
    void ColorCubeChangedDiff(const std::string& cube_name);
    void BasePreviewWidthChangedDiff(Pixel base_preview_width);
    void MaxDPIChangedDiff(PixelDensity dpi);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void CardSizeChanged();
    void BleedChanged();
    void ColorCubeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();

  private:
    const Project& m_Project;
};
