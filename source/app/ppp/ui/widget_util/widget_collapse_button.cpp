#include <ppp/ui/widget_util/widget_collapse_button.hpp>

#include <QLayout>
#include <QPropertyAnimation>
#include <QSize>
#include <QWidget>
#include <QWidgetItem>

CollapsibleLayoutItem::CollapsibleLayoutItem(QWidget* widget)
    : QWidgetItem{ widget }
{
}

float CollapsibleLayoutItem::fadeOut() const
{
    return m_FadeOut;
}
void CollapsibleLayoutItem::setFadeOut(float fade_out)
{
    m_FadeOut = qMax(0.0f, qMin(1.0f, fade_out));

    if (auto* parent{ dynamic_cast<QWidget*>(widget()->parent()) })
    {
        if (auto* parent_layout{ parent->layout() })
        {
            parent_layout->invalidate();
        }
    }
}

QSize CollapsibleLayoutItem::sizeHint() const
{
    return faded(QWidgetItem::sizeHint());
}
QSize CollapsibleLayoutItem::minimumSize() const
{
    return faded(QWidgetItem::minimumSize());
}
QSize CollapsibleLayoutItem::maximumSize() const
{
    return faded(QWidgetItem::maximumSize());
}
QSize CollapsibleLayoutItem::faded(QSize size) const
{
    if (m_FadeOut < 1.0f)
    {
        size.setHeight(static_cast<int>(size.height() * m_FadeOut));
    }
    return size;
}

bool CollapsibleLayoutItem::hasHeightForWidth() const
{
    if (m_FadeOut < 1.0f)
    {
        return false;
    }
    return QWidgetItem::hasHeightForWidth();
}
int CollapsibleLayoutItem::heightForWidth(int width) const
{
    if (m_FadeOut < 1.0f)
    {
        return maximumSize().height();
    }
    return QWidgetItem::heightForWidth(width);
}

void CollapsibleLayoutItem::setGeometry(const QRect& rect)
{
    if (QWidget* w = widget())
    {
        const auto preferred_size{ w->sizeHint() };
        const QRect preferred_rect{
            rect.x(),
            rect.y(),
            rect.width(),
            preferred_size.height(),
        };
        w->setGeometry(preferred_rect);

        if (m_FadeOut < 1.0f)
        {
            const QRect mask{ 0, 0, rect.width(), qMax(1, rect.height()) };
            w->setMask(mask);
        }
        else
        {
            w->clearMask();
        }
    }
}

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

    m_LayoutItem = new CollapsibleLayoutItem{ m_HandledWidget };

    m_Animation = new QPropertyAnimation{ m_LayoutItem, "fadeOut" };
    m_Animation->setStartValue(0.0f);
    m_Animation->setEndValue(1.0f);
    m_Animation->setEasingCurve(QEasingCurve::InOutQuad);
    m_Animation->setDuration(300);
    m_Animator.addAnimation(m_Animation);

    if (collapsed)
    {
        setChecked(false);
        setArrowType(Qt::ArrowType::RightArrow);
        m_LayoutItem->setFadeOut(0.0f);
    }
    else
    {
        blockSignals(true);
        setChecked(true);
        blockSignals(false);
        setArrowType(Qt::ArrowType::DownArrow);
        m_Animation->setCurrentTime(m_Animation->totalDuration());
    }
}

QWidgetItem* CollapseButton::GetLayoutItem()
{
    return m_LayoutItem;
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
