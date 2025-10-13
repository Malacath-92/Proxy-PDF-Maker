#pragma once

#include <QWidget>

class QProgressBar;

class Project;

class ActionsWidget : public QWidget
{
    Q_OBJECT

  public:
    ActionsWidget(Project& project);

  signals:
    void NewProjectOpened();
    void ImageDirChanged();

  public slots:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

  private:
    static inline constexpr int c_ProgressBarResolution{ 250 };

    QProgressBar* m_CropperProgressBar{ nullptr };
    QWidget* m_RenderButton{ nullptr };
};
