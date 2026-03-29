#pragma once

#include <QPainterPath>
#include <QWidget>

class Project;

class MarginsOverlay : public QWidget
{
  public:
    MarginsOverlay(const Project& project, bool is_backside);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    const Project& m_Project;
    const bool m_IsBackside;

    QPainterPath m_Margins;
};
