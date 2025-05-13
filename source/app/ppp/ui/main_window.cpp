#include <ppp/ui/main_window.hpp>

#include <QCloseEvent>
#include <QHBoxLayout>

#include <ppp/ui/popups.hpp>

PrintProxyPrepMainWindow::PrintProxyPrepMainWindow(MainTabs* tabs)
{
    setWindowTitle("PDF Proxy Printer");
    setCentralWidget(tabs);
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
