#pragma once

#include <QWidget>

class QPushButton;
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

    void RenderBackendChanged();

  private:
    static inline constexpr int c_ProgressBarResolution{ 250 };

    QProgressBar* m_CropperProgressBar{ nullptr };
    QPushButton* m_RenderButton{ nullptr };
};
