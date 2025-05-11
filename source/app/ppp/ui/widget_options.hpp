#pragma once

#include <QScrollArea>

class PrintProxyPrepApplication;
class ActionsWidget;
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

  public slots:
    void CropperWorking();
    void CropperDone();
    void CropperProgress(float progress);

  private:
    ActionsWidget* Actions{ nullptr };
    PrintOptionsWidget* PrintOptions{ nullptr };
    CardOptionsWidget* CardOptions{ nullptr };
};
