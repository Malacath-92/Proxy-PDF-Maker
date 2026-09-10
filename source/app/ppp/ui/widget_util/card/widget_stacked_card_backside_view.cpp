#include <ppp/ui/widget_util/card/widget_stacked_card_backside_view.hpp>

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStackedLayout>

#include <ppp/ui/widget_util/card/widget_card_image.hpp>
#include <ppp/ui/widget_util/card/widget_clearable_card_image.hpp>

#include <ppp/ui/view_models/util.hpp>
#include <ppp/ui/view_models/view_model_blank_card.hpp>

#include <ppp/profile/profile.hpp>

StackedCardBacksideView::StackedCardBacksideView(BlankCardViewModel* view_model,
                                                 CardImage* image,
                                                 ClearableCardImage* backside)
    : WidgetWithCardSize{ image->GetAspectRatio() }
    , m_ViewModel{ *view_model }
    , m_Image{ image }
    , m_Backside{ backside }
{
    TRACY_AUTO_SCOPE();

    m_ViewModel.setParent(this);

    m_Backside->setToolTip("Choose individual Backside");

    auto* backside_layout{ new QHBoxLayout };
    backside_layout->addStretch();
    backside_layout->addWidget(m_Backside, 0, Qt::AlignmentFlag::AlignBottom);
    backside_layout->setContentsMargins(0, 0, 0, 0);

    m_BacksideContainer = new QWidget{ this };
    m_BacksideContainer->setLayout(backside_layout);

    m_Image->setMouseTracking(true);
    m_Backside->setMouseTracking(true);
    m_BacksideContainer->setMouseTracking(true);
    setMouseTracking(true);

    addWidget(m_Image);
    addWidget(m_BacksideContainer);

    auto* this_layout{ static_cast<QStackedLayout*>(layout()) };
    this_layout->setStackingMode(QStackedLayout::StackingMode::StackAll);
    this_layout->setAlignment(m_Image, Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);
    this_layout->setAlignment(m_Backside, Qt::AlignmentFlag::AlignBottom | Qt::AlignmentFlag::AlignRight);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardAspectRatioChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(MinimumWidthChanged);

    m_ViewModel.EmitDefaults(false);
}

void StackedCardBacksideView::RefreshBackside(OptionalImageRef backside)
{
    TRACY_AUTO_SCOPE();

    if (backside.has_value())
    {
        m_Backside->SetCardName(backside.value());
    }
    else
    {
        m_Backside->Clear();
    }

    RefreshSizes(rect().size());
}

void StackedCardBacksideView::CardAspectRatioChanged(float aspect_ratio)
{
    WidgetWithCardSize::ChangeAspectRatio(aspect_ratio);
}

void StackedCardBacksideView::MinimumWidthChanged(Pixel minimum_width)
{
    setMinimumWidth(minimum_width / 1_pix);
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
