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
    Q_OBJECT

  public:
    PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options);

    void OpenAboutPopup();

    void Toast(ToastType type,
               QString title,
               QString message);

    virtual void closeEvent(QCloseEvent* event) override;

    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

  signals:
    void ImageDropped(const fs::path& absolute_image_path) const;
};
