#include <ppp/ui/preview/overlays/widget_borders_overlay.hpp>

#include <QPainter>
#include <QResizeEvent>

#include <ppp/svg/generate.hpp>

#include <ppp/ui/view_models/overlays/view_model_borders_overlay.hpp>

BordersOverlay::BordersOverlay(BordersOverlayViewModel* view_model,
                               const PageImageTransforms& transforms)
    : m_ViewModel{ *view_model }
    , m_Transforms{ transforms }
{
    view_model->setParent(this);

    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void BordersOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{ this };
    DrawSvg(painter, m_CardBorder);
    painter.end();
}

void BordersOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const dla::ivec2 size{ event->size().width(), event->size().height() };
    const auto page_size{ m_ViewModel.GetPageSize() };
    const auto pixel_ratio{ size / page_size };

    const auto corner_radius{ m_ViewModel.GetCardCornerRadius() * pixel_ratio };

    m_CardBorder.clear();

    if (m_ViewModel.ShouldDrawOuterBorder())
    {
        const auto margins{ m_ViewModel.GetPageMargins() };
        const auto cards_size{ m_ViewModel.GetCardsSize() };
        const Size available_space{
            page_size.x - margins.m_Left - margins.m_Right,
            page_size.y - margins.m_Top - margins.m_Bottom,
        };
        const Position cards_origin{
            margins.m_Left + (available_space.x - cards_size.x) / 2.0f,
            margins.m_Top + (available_space.y - cards_size.y) / 2.0f,
        };

        const QRectF rect{
            cards_origin.x * pixel_ratio.x,
            cards_origin.y * pixel_ratio.y,
            cards_size.x * pixel_ratio.x,
            cards_size.y * pixel_ratio.y,
        };
        m_CardBorder.addRect(rect);
    }

    for (const auto& transform : m_Transforms)
    {
        if (m_ViewModel.ShouldDrawCardSvg())
        {
            m_ViewModel.DrawCardSvg(m_CardBorder,
                                    transform,
                                    1.0f / pixel_ratio);
        }
        else
        {
            const auto top_left_corner{ transform.m_Card.m_Position * pixel_ratio };
            const auto card_size{ transform.m_Card.m_Size * pixel_ratio };

            const QRectF rect{
                top_left_corner.x,
                top_left_corner.y,
                card_size.x,
                card_size.y,
            };
            if (m_ViewModel.ShouldDrawRoundedRect())
            {
                m_CardBorder.addRoundedRect(rect, corner_radius.x, corner_radius.y);
            }
            else
            {
                m_CardBorder.addRect(rect);
            }
        }
    }
}
