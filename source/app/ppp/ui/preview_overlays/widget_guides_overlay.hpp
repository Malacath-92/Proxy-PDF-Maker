#pragma once

#include <QLineF>
#include <QList>
#include <QPen>
#include <QWidget>

#include <ppp/pdf/util.hpp>

class Project;

class GuidesOverlay : public QWidget
{
  public:
    GuidesOverlay(const Project& project, const PageImageTransforms& transforms);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    const Project& m_Project;
    const PageImageTransforms& m_Transforms;

    QPen m_PenOne;
    QPen m_PenTwo;
    QList<QLineF> m_Lines;
};
