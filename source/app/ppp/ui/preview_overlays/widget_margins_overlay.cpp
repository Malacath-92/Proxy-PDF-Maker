#include <ppp/ui/preview_overlays/widget_margins_overlay.hpp>

#include <QResizeEvent>

#include <ppp/project/project.hpp>
#include <ppp/svg/generate.hpp>

MarginsOverlay::MarginsOverlay(const Project& project)
    : m_Project{ project }
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void MarginsOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{ this };
    DrawSvg(painter, m_Margins, QColor{ 0, 0, 255 });
    painter.end();
}

void MarginsOverlay::resizeEvent(QResizeEvent* event)
{
    const dla::ivec2 size{ event->size().width(), event->size().height() };
    const auto pixel_ratio{ size.x / m_Project.ComputePageSize().x };

    const auto margins{
        m_Project.ComputeMargins() * pixel_ratio
    };
    const auto page_size{
        m_Project.ComputePageSize() * pixel_ratio
    };

    m_Margins.clear();

    m_Margins.moveTo(0, margins.m_Top);
    m_Margins.lineTo(page_size.x, margins.m_Top);

    m_Margins.moveTo(0, page_size.y - margins.m_Bottom);
    m_Margins.lineTo(page_size.x, page_size.y - margins.m_Bottom);

    m_Margins.moveTo(margins.m_Left, 0);
    m_Margins.lineTo(margins.m_Left, page_size.y);

    m_Margins.moveTo(page_size.x - margins.m_Right, 0);
    m_Margins.lineTo(page_size.x - margins.m_Right, page_size.y);
}
