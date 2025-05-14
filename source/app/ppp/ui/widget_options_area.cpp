#include <ppp/ui/widget_options_area.hpp>

#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

class CollapseButton : public QToolButton
{
  public:
    CollapseButton(QWidget* handled_widget, bool collapsed)
        : m_HandledWidget{ handled_widget }
    {
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setCheckable(true);
        setStyleSheet("background:none; border:none");
        setIconSize(QSize(8, 8));
        setText(" " + handled_widget->objectName());
        connect(this,
                &QToolButton::toggled,
                [this](bool checked)
                {
                    setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
                    m_HandledWidget != nullptr&& checked
                        ? Check()
                        : Uncheck();
                });

        auto animation{ new QPropertyAnimation{ m_HandledWidget, "maximumHeight" } };
        animation->setStartValue(0);
        animation->setEasingCurve(QEasingCurve::InOutQuad);
        animation->setDuration(300);
        animation->setEndValue(m_HandledWidget->geometry().height() + 10);
        m_Animator.addAnimation(animation);

        if (collapsed)
        {
            setChecked(false);
            setArrowType(Qt::ArrowType::RightArrow);
            m_HandledWidget->setMaximumHeight(0);
        }
        else
        {
            setChecked(true);
            setArrowType(Qt::ArrowType::DownArrow);
        }
    }

    void Uncheck()
    {
        m_Animator.setDirection(QAbstractAnimation::Backward);
        m_Animator.start();
    }

    void Check()
    {
        m_Animator.setDirection(QAbstractAnimation::Forward);
        m_Animator.start();
    }

  private:
    QWidget* m_HandledWidget;
    QParallelAnimationGroup m_Animator;
};

OptionsAreaWidget::OptionsAreaWidget(QWidget* actions,
                                     QWidget* print_options,
                                     QWidget* guides_options,
                                     QWidget* card_options,
                                     QWidget* global_options)
{
    constexpr auto add_collapsable{
        [](QVBoxLayout* layout, QWidget* widget, bool default_collapsed)
        {
            auto* collapse_button{ new CollapseButton{ widget, default_collapsed } };
            layout->addWidget(collapse_button);
            layout->addWidget(widget);
        }
    };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(actions);
    add_collapsable(layout, print_options, false);
    add_collapsable(layout, guides_options, true);
    add_collapsable(layout, card_options, false);
    add_collapsable(layout, global_options, true);
    layout->addStretch();

    auto* widget{ new QWidget };
    widget->setLayout(layout);
    widget->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::MinimumExpanding);

    setWidget(widget);
    setWidgetResizable(true);
    setMinimumHeight(400);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
}