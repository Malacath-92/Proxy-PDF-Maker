#include <ppp/ui/widget_tabs.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_print_preview.hpp>
#include <ppp/ui/widget_scroll_area.hpp>

MainTabs::MainTabs(const Project& project, CardScrollArea* scroll_area, PrintPreview* print_preview)
{
    addTab(scroll_area, "Cards");
    addTab(print_preview, "Preview");

    auto current_changed{
        [=, &project](int i)
        {
            if (widget(i) == print_preview)
            {
                print_preview->Refresh(project);
            }
        }
    };

    QObject::connect(this,
                     &QTabWidget::currentChanged,
                     this,
                     current_changed);
}