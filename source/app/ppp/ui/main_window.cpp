#include <ppp/ui/main_window.hpp>

#include <QHBoxLayout>

PrintProxyPrepMainWindow::PrintProxyPrepMainWindow(Project& project, MainTabs* tabs, CardScrollArea* scroll, PrintPreview* preview, OptionsWidget* options)
    : AppProject{ project }
    , Scroll{ scroll }
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

void PrintProxyPrepMainWindow::Refresh()
{
    Scroll->Refresh(AppProject);
    Options->Refresh(AppProject);
    RefreshWidgets();
}

void PrintProxyPrepMainWindow::RefreshWidgets()
{
    Options->RefreshWidgets(AppProject);
}

void PrintProxyPrepMainWindow::RefreshPreview()
{
    Preview->Refresh(AppProject);
}
