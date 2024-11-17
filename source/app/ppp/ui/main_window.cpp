#include <ppp/ui/main_window.hpp>

#include <QHBoxLayout>

PrintProxyPrepMainWindow::PrintProxyPrepMainWindow(MainTabs* tabs, CardScrollArea* scroll, PrintPreview* preview, OptionsWidget* options)
    : Scroll{ scroll }
    , Preview{ preview }
    , Options{ options }
{
    setWindowTitle("PDF Proxy Printer");

    auto* window_layout{ new QHBoxLayout };
    window_layout->addWidget(tabs);
    window_layout->addWidget(options);

    auto* window_area{ new QWidget };
    window_area->setLayout(window_layout);

    setCentralWidget(window_area);
}

void PrintProxyPrepMainWindow::Refresh(const Project& project)
{
    // Scroll->Refresh(project);
    // Options->Refresh(project);
    RefreshWidgets(project);
}

void PrintProxyPrepMainWindow::RefreshWidgets(const Project& /*project*/)
{
    // Options->RefreshWidgets(project);
}

void PrintProxyPrepMainWindow::RefreshPreview(const Project& /*project*/)
{
    // Preview->Refresh(project);
}
