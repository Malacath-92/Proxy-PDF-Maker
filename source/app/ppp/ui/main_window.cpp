#include <ppp/ui/main_window.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMimeData>
#include <QStyleHints>

#include <Toast.h>

#include <ppp/project/image_ops.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/app.hpp>
#include <ppp/ui/popups.hpp>

#include <ppp/profile/profile.hpp>

std::array g_ValidDropExtensions{
    []()
    {
        const std::array non_image_drop_extensions{
            ".pdf"_p,
            ".CUBE"_p,
            ".qss"_p,
            ".svg"_p,
        };

        constexpr auto total_drop_extensions{
            g_ValidImageExtensions.size() + non_image_drop_extensions.size()
        };
        std::array<fs::path, total_drop_extensions> valid_drop_extensions{};
        std::copy(g_ValidImageExtensions.begin(),
                  g_ValidImageExtensions.end(),
                  valid_drop_extensions.begin());
        std::copy(non_image_drop_extensions.begin(),
                  non_image_drop_extensions.end(),
                  valid_drop_extensions.begin() + g_ValidImageExtensions.size());
        return valid_drop_extensions;
    }()
};

PrintProxyPrepMainWindow::PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options)
{
    TRACY_AUTO_SCOPE();

    ProjectPathChanged();

    auto* window_layout{ new QHBoxLayout };
    window_layout->addWidget(tabs);
    window_layout->addWidget(options);

    auto* window_area{ new QWidget };
    window_area->setLayout(window_layout);

    setCentralWidget(window_area);

    {
        const auto* primary_screen{ qApp->primaryScreen() };
        const auto geometry{ primary_screen->geometry() };
        const auto available_geometry{ primary_screen->availableGeometry() };
        Toast::setOffsetY(geometry.height() - available_geometry.bottom());
    }
}

void PrintProxyPrepMainWindow::OpenAboutPopup()
{
    if (!isEnabled())
    {
        return;
    }

    AboutPopup about{ nullptr };

    setEnabled(false);
    about.Show();
    setEnabled(true);
}

void PrintProxyPrepMainWindow::Toast(ToastType type,
                                     QString title,
                                     QString message)
{
    TRACY_AUTO_SCOPE();

    Q_INIT_RESOURCE(toast_resources);

    auto* toast{ new ::Toast };
    toast->setDuration(8000);
    toast->setTitle(std::move(title));
    toast->setRichText(std::move(message));
    toast->setPosition(ToastPosition::BOTTOM_LEFT);

    const bool dark_mode{
        QApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
    };
    switch (type)
    {
    case ToastType::Info:
        toast->applyPreset(dark_mode ? ToastPreset::INFORMATION_DARK
                                     : ToastPreset::INFORMATION);
        break;
    case ToastType::Warning:
        toast->applyPreset(dark_mode ? ToastPreset::WARNING_DARK
                                     : ToastPreset::WARNING);
        break;
    case ToastType::Error:
        toast->applyPreset(dark_mode ? ToastPreset::ERROR_DARK
                                     : ToastPreset::ERROR);
        break;
    }

    toast->show();
}
void PrintProxyPrepMainWindow::ImageDropRejected(const fs::path& absolute_image_path)
{
    Toast(ToastType::Info,
          "Drag 'N Drop Failed",
          QString{ "An image with name %1 is already part of the project." }
              .arg(ToQString(absolute_image_path.filename())));
}

void PrintProxyPrepMainWindow::closeEvent(QCloseEvent* event)
{
    if (isEnabled())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void PrintProxyPrepMainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    TRACY_AUTO_SCOPE();

    if (event->mimeData()->hasFormat("text/uri-list"))
    {
        const auto uri_list{ event->mimeData()->text().split('\n') };
        for (const auto& uri : uri_list)
        {
            static constexpr std::string_view c_FileUriStart{ "file:///" };
            if (uri.startsWith(c_FileUriStart.data()) &&
                std::ranges::contains(g_ValidDropExtensions,
                                      fs::path{ uri.toStdString() }.extension()))
            {
                // We have at least one valid image file in this
                event->acceptProposedAction();
                break;
            }
        }
    }

    QMainWindow::dragEnterEvent(event);
}

void PrintProxyPrepMainWindow::dropEvent(QDropEvent* event)
{
    TRACY_AUTO_SCOPE();

    const auto uri_list{ event->mimeData()->text().split('\n') };
    for (const auto& uri : uri_list)
    {
        static constexpr std::string_view c_FileUriStart{ "file:///" };
        if (uri.startsWith(c_FileUriStart.data()))
        {
            const fs::path path{ uri.sliced(c_FileUriStart.size()).toStdString() };
            const auto ext{ path.extension() };
            if (ext == ".pdf")
            {
                PdfDropped(path);
            }
            else if (ext == ".CUBE")
            {
                ColorCubeDropped(path);
            }
            else if (ext == ".qss")
            {
                StyleDropped(path);
            }
            else if (std::ranges::contains(g_ValidImageExtensions, ext))
            {
                ImageDropped(path);
            }
            else if (ext == ".svg")
            {
                QMessageBox::StandardButton reply{
                    QMessageBox::question(this,
                                          "SVG Import",
                                          "Do you want to import this .svg file as a custom card size?",
                                          QMessageBox::Yes | QMessageBox::No)
                };
                if (reply == QMessageBox::Yes)
                {
                    SvgDropped(path);
                }
            }
        }
    }

    QMainWindow::dropEvent(event);
}

void PrintProxyPrepMainWindow::ProjectPathChanged()
{
    const auto* app{ static_cast<const PrintProxyPrepApplication*>(qApp) };
    setWindowTitle(QString{ "Proxy-PDF-Maker - %1" }
                       .arg(ToQString(app->GetProjectPath().filename())));
}
