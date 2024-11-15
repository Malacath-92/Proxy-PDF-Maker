#pragma once

#include <QWidget>

class PrintProxyPrepApplication;
class PrintOptionsWidget;
class CardOptionsWidget;
struct Project;

class OptionsWidget : public QWidget
{
  public:
    OptionsWidget(PrintProxyPrepApplication& application, Project& project);

    void Refresh(const Project& project);
    void RefreshWidgets(const Project& project);

  private:
    PrintOptionsWidget* PrintOptions{ nullptr };
    CardOptionsWidget* CardOptions{ nullptr };
};
