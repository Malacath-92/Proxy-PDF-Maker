#pragma once

#include <QWidget>

class Project;

class GuidesOptionsWidget : public QWidget
{
  public:
    GuidesOptionsWidget(Project& project);

  private:
    Project& m_Project;
};
