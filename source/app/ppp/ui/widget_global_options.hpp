#pragma once

#include <QWidget>

class PrintProxyPrepApplication;
class Project;

class GlobalOptionsWidget : public QWidget
{
  public:
    GlobalOptionsWidget(PrintProxyPrepApplication& application, Project& project);
};
