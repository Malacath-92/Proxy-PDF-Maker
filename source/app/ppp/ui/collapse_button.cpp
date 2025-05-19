#include <ppp/ui/collapse_button.hpp>

#include <QPropertyAnimation>

CollapseButton::CollapseButton(QWidget* handled_widget, bool collapsed)
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

void CollapseButton::Uncheck()
{
    m_Animator.setDirection(QAbstractAnimation::Backward);
    m_Animator.start();
    SetObjectVisibility(false);
}

void CollapseButton::Check()
{
    m_Animator.setDirection(QAbstractAnimation::Forward);
    m_Animator.start();
    SetObjectVisibility(true);
}
