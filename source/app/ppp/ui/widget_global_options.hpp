#pragma once

#include <QDockWidget>

class PrintProxyPrepApplication;
class Project;

class GlobalOptionsWidget : public QDockWidget
{
  public:
    GlobalOptionsWidget(PrintProxyPrepApplication& application, Project& project);
};
