#include <ppp/ui/preview/overlays/widget_margins_overlay.hpp>

#include <QResizeEvent>

#include <ppp/project/project.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/ui/view_models/overlays/view_model_margins_overlay.hpp>

MarginsOverlay::MarginsOverlay(MarginsOverlayViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    view_model->setParent(this);

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
    const auto raw_page_size{ m_ViewModel.GetPageSize() };
    const auto raw_page_margins{ m_ViewModel.GetPageMargins() };
    const auto pixel_ratio{ size.x / raw_page_size.x };

    const auto page_margins{ raw_page_margins * pixel_ratio };
    const auto page_size{ raw_page_size * pixel_ratio };

    m_Margins.clear();

    m_Margins.moveTo(0, page_margins.m_Top);
    m_Margins.lineTo(page_size.x, page_margins.m_Top);

    m_Margins.moveTo(0, page_size.y - page_margins.m_Bottom);
    m_Margins.lineTo(page_size.x, page_size.y - page_margins.m_Bottom);

    m_Margins.moveTo(page_margins.m_Left, 0);
    m_Margins.lineTo(page_margins.m_Left, page_size.y);

    m_Margins.moveTo(page_size.x - page_margins.m_Right, 0);
    m_Margins.lineTo(page_size.x - page_margins.m_Right, page_size.y);
}
