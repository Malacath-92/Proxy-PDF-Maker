#include <ppp/ui/widget_util/widget_collapse_button.hpp>

#include <QPropertyAnimation>

CollapseButton::CollapseButton(QWidget* handled_widget, bool collapsed)
    : m_HandledWidget{ handled_widget }
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setCheckable(true);
    setStyleSheet("background:none; border:none");
    setIconSize(QSize(8, 8));
    setText(" " + m_HandledWidget->objectName());
    connect(this,
            &QToolButton::toggled,
            [this](bool checked)
            {
                setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
                m_HandledWidget != nullptr&& checked
                    ? Check()
                    : Uncheck();
            });

    m_Animation = new QPropertyAnimation{ m_HandledWidget, "maximumHeight" };
    m_Animation->setStartValue(0);
    m_Animation->setEndValue(0);
    m_Animation->setEasingCurve(QEasingCurve::InOutQuad);
    m_Animation->setDuration(300);
    m_Animator.addAnimation(m_Animation);

    if (collapsed)
    {
        setChecked(false);
        setArrowType(Qt::ArrowType::RightArrow);
        m_HandledWidget->setMaximumHeight(0);
    }
    else
    {
        blockSignals(true);
        setChecked(true);
        blockSignals(false);
        setArrowType(Qt::ArrowType::DownArrow);
    }
}

void CollapseButton::Uncheck()
{
    if (m_Animation->endValue() == 0)
    {
        m_Animation->setEndValue(m_HandledWidget->sizeHint().height());
        m_Animation->setCurrentTime(m_Animation->totalDuration());
    }

    m_Animator.setDirection(QAbstractAnimation::Backward);
    m_Animator.start();
    SetObjectVisibility(false);
}

void CollapseButton::Check()
{
    m_Animation->setEndValue(m_HandledWidget->sizeHint().height());

    m_Animator.setDirection(QAbstractAnimation::Forward);
    m_Animator.start();
    SetObjectVisibility(true);
}
