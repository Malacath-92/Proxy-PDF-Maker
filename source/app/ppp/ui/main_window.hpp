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

    void ImageDropRejected(const fs::path& absolute_image_path);

    virtual void closeEvent(QCloseEvent* event) override;

    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

  signals:
    void PdfDropped(const fs::path& absolute_pdf_path) const;
    void ColorCubeDropped(const fs::path& absolute_cube_path) const;
    void StyleDropped(const fs::path& absolute_qss_path) const;
    void ImageDropped(const fs::path& absolute_image_path) const;
};
