#pragma once

#include <QScrollArea>

class PrintProxyPrepApplication;
class PrintOptionsWidget;
class CardOptionsWidget;
class Project;

class OptionsWidget : public QScrollArea
{
  public:
    OptionsWidget(PrintProxyPrepApplication& application, Project& project);

    void Refresh();
    void RefreshWidgets();

    void BaseUnitChanged();

  private:
    PrintOptionsWidget* PrintOptions{ nullptr };
    CardOptionsWidget* CardOptions{ nullptr };
};
