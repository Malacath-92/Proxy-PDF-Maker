#pragma once

#include <QScrollArea>

class PrintProxyPrepApplication;
class Project;

class OptionsWidget : public QScrollArea
{
  public:
    OptionsWidget(PrintProxyPrepApplication& application, Project& project);
};
