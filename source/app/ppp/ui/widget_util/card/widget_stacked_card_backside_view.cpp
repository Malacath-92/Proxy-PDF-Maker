#include <ppp/ui/widget_util/card/widget_stacked_card_backside_view.hpp>

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStackedLayout>

#include <ppp/ui/widget_util/card/widget_card_image.hpp>
#include <ppp/ui/widget_util/card/widget_clearable_card_image.hpp>

#include <ppp/profile/profile.hpp>

StackedCardBacksideView::StackedCardBacksideView(CardImage* image, ClearableCardImage* backside)
    : WidgetWithCardSize{ image->GetAspectRatio() }
    , m_Image{ image }
    , m_Backside{ backside }
{
    TRACY_AUTO_SCOPE();

    backside->setToolTip("Choose individual Backside");

    auto* backside_layout{ new QHBoxLayout };
    backside_layout->addStretch();
    backside_layout->addWidget(backside, 0, Qt::AlignmentFlag::AlignBottom);
    backside_layout->setContentsMargins(0, 0, 0, 0);

    m_BacksideContainer = new QWidget{ this };
    m_BacksideContainer->setLayout(backside_layout);

    image->setMouseTracking(true);
    backside->setMouseTracking(true);
    m_BacksideContainer->setMouseTracking(true);
    setMouseTracking(true);

    addWidget(image);
    addWidget(m_BacksideContainer);

    auto* this_layout{ static_cast<QStackedLayout*>(layout()) };
    this_layout->setStackingMode(QStackedLayout::StackingMode::StackAll);
    this_layout->setAlignment(image, Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);
    this_layout->setAlignment(backside, Qt::AlignmentFlag::AlignBottom | Qt::AlignmentFlag::AlignRight);
}

void StackedCardBacksideView::RefreshBackside(OptionalImageRef backside)
{
    TRACY_AUTO_SCOPE();

    m_Backside->Refresh(backside, true);

    RefreshSizes(rect().size());
}

void StackedCardBacksideView::RefreshSize(const Project& project)
{
    m_Image->RefreshSize(project);

    /*if (auto* image_widget{ dynamic_cast<CardImage*>(m_Backside) })
    {
        image_widget->RefreshSize(project);
    }
    else if (auto* blank_widget{ dynamic_cast<BlankCardImage*>(m_Backside) })
    {
        blank_widget->RefreshSize(project);
    }*/
}

void StackedCardBacksideView::RefreshSizes(QSize size)
{
    const auto width{ size.width() };
    const auto height{ size.height() };

    const auto img_width{ int(width * 0.9) };
    const auto img_height{ int(height * 0.9) };

    const auto backside_width{ int(width * 0.45) };
    const auto backside_height{ int(height * 0.45) };

    m_Image->setFixedWidth(img_width);
    m_Image->setFixedHeight(img_height);
    m_Backside->setFixedWidth(backside_width);
    m_Backside->setFixedHeight(backside_height);
}

void StackedCardBacksideView::resizeEvent(QResizeEvent* event)
{
    QStackedWidget::resizeEvent(event);
    RefreshSizes(event->size());
}

void StackedCardBacksideView::mouseMoveEvent(QMouseEvent* event)
{
    QStackedWidget::mouseMoveEvent(event);

    const auto x{ event->pos().x() };
    const auto y{ event->pos().y() };

    const auto neg_backside_width{ rect().width() - m_Backside->rect().size().width() };
    const auto neg_backside_height{ rect().height() - m_Backside->rect().size().height() };

    if (x >= neg_backside_width && y >= neg_backside_height)
    {
        setCurrentWidget(m_BacksideContainer);
    }
    else
    {
        setCurrentWidget(m_Image);
    }
}

void StackedCardBacksideView::leaveEvent(QEvent* event)
{
    QStackedWidget::leaveEvent(event);

    setCurrentWidget(m_Image);
}

void StackedCardBacksideView::mouseReleaseEvent(QMouseEvent* event)
{
    QStackedWidget::mouseReleaseEvent(event);

    if (!event->isAccepted() &&
        event->button() == Qt::MouseButton::LeftButton &&
        currentWidget() == m_BacksideContainer)
    {
        BacksideClicked();
        event->accept();
    }
}
