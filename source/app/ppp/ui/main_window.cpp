#include <ppp/ui/main_window.hpp>

#include <QCloseEvent>
#include <QHBoxLayout>

#include <ppp/ui/popups.hpp>

PrintProxyPrepMainWindow::PrintProxyPrepMainWindow(QWidget* tabs, QWidget* options)
{
    setWindowTitle("PDF Proxy Printer");

    auto* window_layout{ new QHBoxLayout };
    window_layout->addWidget(tabs);
    window_layout->addWidget(options);

    auto* window_area{ new QWidget };
    window_area->setLayout(window_layout);

    setCentralWidget(window_area);
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
