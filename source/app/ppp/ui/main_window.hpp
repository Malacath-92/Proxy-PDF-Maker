#pragma once

#include <functional>

#include <QMainWindow>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_tabs.hpp>

enum class ToastType
{
    Info,
    Warning,
    Error,
};

class ToastHandler : public QObject
{
    Q_OBJECT

  public:
    virtual bool hasDynamicText() const = 0;

    virtual bool hasOnLink() const = 0;
    virtual bool onLink(const QString& link) = 0;

    virtual bool hasProgress() const = 0;

  signals:
    void textChanged(QString text) const;
    void progress(float progress) const;
};

struct ToastData
{
    ToastType m_Type{ ToastType::Info };
    QString m_Title{ "Toast" };
    QString m_Message{ "Message" };
    ToastHandler* m_Handler{ nullptr };
    bool m_HandlerExternallyOwned{ false };
};

class PrintProxyPrepMainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    PrintProxyPrepMainWindow(QWidget* tabs,
                             QWidget* options,
                             const Config& config);

    void OpenAboutPopup(const Project& project);

    void Toast(ToastData toast_data);

    using OnLinkFn = std::function<bool(const QString& link)>;
    void Toast(ToastType type,
               QString title,
               QString message,
               OnLinkFn on_link = nullptr);

    void ImageDropRejected(const fs::path& absolute_image_path);

    virtual void closeEvent(QCloseEvent* event) override;

    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dropEvent(QDropEvent* event) override;

    void ProjectPathChanged(const fs::path& project_path);

  signals:
    void PdfDropped(const fs::path& absolute_pdf_path) const;
    void ColorCubeDropped(const fs::path& absolute_cube_path) const;
    void StyleDropped(const fs::path& absolute_qss_path) const;
    void ModelDropped(const fs::path& absolute_onnx_path) const;
    void ImageDropped(const fs::path& absolute_image_path) const;
    void SvgDropped(const fs::path& absolute_svg_path) const;

  private:
    const Config& m_Cfg;
};
