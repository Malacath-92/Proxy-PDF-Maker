#include <ppp/ui/preview_overlays/widget_borders_overlay.hpp>

#include <QPainter>
#include <QResizeEvent>

#include <ppp/project/project.hpp>
#include <ppp/svg/generate.hpp>

BordersOverlay::BordersOverlay(const Project& project, const PageImageTransforms& transforms)
    : m_Project{ project }
    , m_Transforms{ transforms }
{
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
    const auto page_size{ m_Project.ComputePageSize() };
    const auto pixel_ratio{ size / page_size };

    const auto bleed_edge{ m_Project.m_Data.m_BleedEdge * pixel_ratio };
    const auto corner_radius{ m_Project.CardCornerRadius() * pixel_ratio };

    m_CardBorder.clear();

    if (m_Project.m_Data.m_BleedEdge > 0_mm)
    {
        const auto margins{ m_Project.ComputeMargins() };
        const auto cards_size{ m_Project.ComputeCardsSize() };
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
        const auto top_left_corner{ transform.m_Position * pixel_ratio };
        const auto card_size{ transform.m_Size * pixel_ratio };

        const QRectF rect{
            top_left_corner.x + bleed_edge.x,
            top_left_corner.y + bleed_edge.y,
            card_size.x - bleed_edge.x * 2.0f,
            card_size.y - bleed_edge.x * 2.0f,
        };
        m_CardBorder.addRoundedRect(rect, corner_radius.x, corner_radius.y);
    }
}
