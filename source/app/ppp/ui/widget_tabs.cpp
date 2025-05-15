#include <ppp/ui/widget_tabs.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_print_preview.hpp>
#include <ppp/ui/widget_scroll_area.hpp>

MainTabs::MainTabs(CardScrollArea* scroll_area, PrintPreview* print_preview)
{
    addTab(scroll_area, "Cards");
    addTab(print_preview, "Preview");

    auto current_changed{
        [=, this](int i)
        {
            if (widget(i) == scroll_area)
            {
                scroll_area->Refresh();
            }
            else
            {
                print_preview->Refresh();
            }
        }
    };

    QObject::connect(this,
                     &QTabWidget::currentChanged,
                     this,
                     current_changed);
}
