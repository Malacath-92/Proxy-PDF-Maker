#pragma once

#include <QWidget>

class QProgressBar;

class PrintProxyPrepApplication;
class Project;

class ActionsWidget : public QWidget
{
  public:
    ActionsWidget(PrintProxyPrepApplication& application, Project& project);

  public slots:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

  private:
    static inline constexpr int c_ProgressBarResolution{ 250 };

    QProgressBar* m_CropperProgressBar{ nullptr };
    QWidget* m_RenderButton{ nullptr };
};
