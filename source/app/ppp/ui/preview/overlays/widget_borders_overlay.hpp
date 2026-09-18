#pragma once

#include <QPainterPath>
#include <QWidget>

#include <ppp/pdf/util.hpp>

class BordersOverlayViewModel;

class BordersOverlay : public QWidget
{
  public:
    BordersOverlay(BordersOverlayViewModel* view_model,
                   const PageImageTransforms& transforms);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private slots:
    void Redraw();

  private:
    const BordersOverlayViewModel& m_ViewModel;
    const PageImageTransforms& m_Transforms;

    QPainterPath m_CardBorder;
};
