#pragma once

#include <QPainterPath>
#include <QWidget>

class MarginsOverlayViewModel;

class MarginsOverlay : public QWidget
{
  public:
    MarginsOverlay(MarginsOverlayViewModel* view_model);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private slots:
    void Redraw();

  private:
    void DrawLines(const QSize& size);

    const MarginsOverlayViewModel& m_ViewModel;

    QPainterPath m_Margins;
};
