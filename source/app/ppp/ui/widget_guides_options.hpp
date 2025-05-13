#pragma once

#include <QDockWidget>

class Project;

class GuidesOptionsWidget : public QDockWidget
{
  public:
    GuidesOptionsWidget(Project& project);

  private:
    Project& m_Project;
};
