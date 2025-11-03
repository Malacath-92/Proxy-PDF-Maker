#include <ppp/ui/main_window.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QStyleHints>

#include <Toast.h>

#include <ppp/ui/popups.hpp>

PrintProxyPrepMainWindow::PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options)
{
    setWindowTitle("Proxy-PDF-Maker");

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
