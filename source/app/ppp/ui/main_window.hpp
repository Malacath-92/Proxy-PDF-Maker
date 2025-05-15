#pragma once

#include <QMainWindow>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_tabs.hpp>

class PrintProxyPrepMainWindow : public QMainWindow
{
  public:
    PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options);

    void OpenAboutPopup();

    virtual void closeEvent(QCloseEvent* event) override;
};
