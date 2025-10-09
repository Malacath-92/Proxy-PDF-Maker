#include <ppp/ui/main_window.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>

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
    auto* toast{ new ::Toast(this) };
    toast->setDuration(8000);
    toast->setTitle(std::move(title));
    toast->setRichText(std::move(message));
    switch (type)
    {
    case ToastType::Info:
        toast->applyPreset(ToastPreset::INFORMATION);
        break;
    case ToastType::Warning:
        toast->applyPreset(ToastPreset::WARNING);
        break;
    case ToastType::Error:
        toast->applyPreset(ToastPreset::ERROR);
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
