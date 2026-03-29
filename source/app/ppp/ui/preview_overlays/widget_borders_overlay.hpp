#pragma once

#include <QPainterPath>
#include <QWidget>

#include <ppp/pdf/util.hpp>

class Project;

class BordersOverlay : public QWidget
{
  public:
    BordersOverlay(const Project& project,
                   const PageImageTransforms& transforms,
                   bool is_backside);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    const Project& m_Project;
    const PageImageTransforms& m_Transforms;
    const bool m_IsBackside;

    QPainterPath m_CardBorder;
};
