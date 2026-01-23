#pragma once

#include <QWidget>

#include <ppp/util.hpp>

class QPushButton;
class QProgressBar;

class Project;
struct ProjectData;

class ActionsWidget : public QWidget
{
    Q_OBJECT

  public:
    ActionsWidget(Project& project);

  signals:
    void NewProjectOpened(const ProjectData& old_project, const ProjectData& new_project);
    void ImageDirChanged(const fs::path& old_path, const fs::path& new_path);

  public slots:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

    void RenderBackendChanged();

  private:
    static inline constexpr int c_ProgressBarResolution{ 250 };

    QProgressBar* m_CropperProgressBar{ nullptr };
    QPushButton* m_RenderButton{ nullptr };
};
