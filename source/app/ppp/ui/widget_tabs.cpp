#include <ppp/ui/widget_tabs.hpp>

#include <QLayout>
#include <QTabBar>

#include <ppp/project/project.hpp>

#include <ppp/ui/options/widget_actions.hpp>
#include <ppp/ui/preview/widget_print_preview.hpp>
#include <ppp/ui/widget_card_area.hpp>

#include <ppp/profile/profile.hpp>

class LargeTabBar : public QTabBar
{
  public:
    LargeTabBar(int extra_space)
        : m_ExtraSpace{ extra_space }
    {
    }

  protected:
    QSize tabSizeHint(int index) const override
    {
        QSize size{ QTabBar::tabSizeHint(index) };
        size.setHeight(size.height() + m_ExtraSpace);
        return size;
    }

  private:
    int m_ExtraSpace;
};

MainTabs::MainTabs(ActionsWidget* actions,
                   CardArea* card_area,
                   PrintPreview* print_preview)
    : m_CardArea{ card_area }
{
    TRACY_AUTO_SCOPE();

    setTabBar(new LargeTabBar{ 6 +
                               actions->contentsMargins().top() +
                               actions->contentsMargins().bottom() });

    setCornerWidget(actions);

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
