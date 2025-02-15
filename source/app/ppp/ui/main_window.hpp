#pragma once

#include <QMainWindow>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_options.hpp>
#include <ppp/ui/widget_print_preview.hpp>
#include <ppp/ui/widget_scroll_area.hpp>
#include <ppp/ui/widget_tabs.hpp>

class PrintProxyPrepMainWindow : public QMainWindow
{
  public:
    PrintProxyPrepMainWindow(Project& project, MainTabs* tabs, CardScrollArea* scroll, PrintPreview* preview, OptionsWidget* options);

    void Refresh();
    void RefreshWidgets();
    void RefreshPreview();

    void OpenAboutPopup();

  private:
    Project& AppProject;

    CardScrollArea* Scroll;
    PrintPreview* Preview;
    OptionsWidget* Options;
};
