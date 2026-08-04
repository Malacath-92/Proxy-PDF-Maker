#pragma once

#include <QWidget>

#include <ppp/config.hpp>
#include <ppp/util.hpp>

class QPushButton;
class QProgressBar;
class QStackedWidget;

class ActionsViewModel;

class ActionsWidget : public QWidget
{
    Q_OBJECT

  public:
    ActionsWidget(ActionsViewModel* view_model,
                  PdfBackend backend);

  private slots:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

    void RenderBackendChanged(PdfBackend backend);

  private:
    void RenderButtonPressed() const;
    void SetImagesButtonPressed() const;

    static inline constexpr int c_ProgressBarResolution{ 250 };

    ActionsViewModel& m_ViewModel;

    QStackedWidget* m_RenderCropperContainer{ nullptr };
    QProgressBar* m_CropperProgressBar{ nullptr };
    QPushButton* m_RenderButton{ nullptr };
};
