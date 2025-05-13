#pragma once

#include <QScrollArea>

class PrintProxyPrepApplication;
class CardOptionsWidget;
class Project;

class OptionsWidget : public QScrollArea
{
  public:
    OptionsWidget(PrintProxyPrepApplication& application, Project& project);

    void RefreshWidgets();

    void BaseUnitChanged();

  private:
    CardOptionsWidget* CardOptions{ nullptr };
};
