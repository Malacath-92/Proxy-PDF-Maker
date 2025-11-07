#pragma once

#include <QPainterPath>
#include <QWidget>

class Project;

class MarginsOverlay : public QWidget
{
  public:
    MarginsOverlay(const Project& project);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    const Project& m_Project;

    QPainterPath m_Margins;
};
