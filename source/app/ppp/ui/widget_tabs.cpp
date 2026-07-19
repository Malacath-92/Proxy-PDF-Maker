#include <ppp/ui/widget_tabs.hpp>

#include <QLayout>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_card_area.hpp>
#include <ppp/ui/widget_print_preview.hpp>

#include <ppp/profile/profile.hpp>

MainTabs::MainTabs(CardArea* card_area, PrintPreview* print_preview)
    : m_CardArea{ card_area }
{
    TRACY_AUTO_SCOPE();

    addTab(card_area, "Cards");
    addTab(print_preview, "Preview");

    auto current_changed{
        [=, this](int i)
        {
            if (widget(i) == print_preview)
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

int MainTabs::MaximumColumnsFromAvailableWidth(int available_width) const
{
    const auto margins{ contentsMargins() };
    available_width -= margins.left() +
                       margins.right();
    return m_CardArea->MaximumColumnsFromAvailableWidth(available_width);
}
