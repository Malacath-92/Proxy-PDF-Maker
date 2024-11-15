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
    PrintProxyPrepMainWindow(MainTabs* tabs, CardScrollArea* scroll, PrintPreview* preview, OptionsWidget* options);

    void Refresh(const Project& project);
    void RefreshWidgets(const Project& project);
    void RefreshPreview(const Project& project);

  private:
    CardScrollArea* Scroll;
    PrintPreview* Preview;
    OptionsWidget* Options;
};
