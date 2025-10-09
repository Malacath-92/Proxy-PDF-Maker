#pragma once

#include <QMainWindow>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_tabs.hpp>

enum class ToastType
{
    Info,
    Warning,
    Error,
};

class PrintProxyPrepMainWindow : public QMainWindow
{
  public:
    PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options);

    void OpenAboutPopup();

    void Toast(ToastType type,
               QString title,
               QString message);

    virtual void closeEvent(QCloseEvent* event) override;
};
