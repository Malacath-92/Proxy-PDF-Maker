#pragma once

#include <QLineF>
#include <QList>
#include <QPen>
#include <QWidget>

#include <ppp/color.hpp>
#include <ppp/pdf/util.hpp>

class GuidesOverlayViewModel;

class GuidesOverlay : public QWidget
{
    Q_OBJECT

  public:
    GuidesOverlay(GuidesOverlayViewModel* view_model, const PageImageTransforms& transforms);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private slots:
    void GuidesColorsChanged(ColorRGB8 color_a, ColorRGB8 color_b);
    void Redraw();

  private:
    void RedrawLines(QSize size);

    const GuidesOverlayViewModel& m_ViewModel;
    const PageImageTransforms& m_Transforms;

    QPen m_PenOne;
    QPen m_PenTwo;

    QList<QLineF> m_SolidLines;
    QList<QLineF> m_DashedLines;
};
