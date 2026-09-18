#include <ppp/ui/view_models/view_model_print_preview.hpp>

#include <QPainter>

#include <ppp/config.hpp>
#include <ppp/pdf/util.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>

#include <ppp/ui/view_models/view_model_page_preview.hpp>

PrintPreviewViewModel::PrintPreviewViewModel(Project& project,
                                             const Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
    m_RefreshTimer.setSingleShot(true);
    m_RefreshTimer.setInterval(50);
    QObject::connect(&m_RefreshTimer,
                     &QTimer::timeout,
                     this,
                     &PrintPreviewViewModel::RequestRefresh);
}

PagePreviewViewModel* PrintPreviewViewModel::MakePagePreviewViewModel(bool is_backside) const
{
    auto* page_preview_view_model{
        new PagePreviewViewModel{ m_Project, m_Cfg, is_backside }
    };
    QObject::connect(this,
                     &PrintPreviewViewModel::PageBackgroundChanged,
                     page_preview_view_model,
                     &PagePreviewViewModel::PageBackgroundChanged);

    return page_preview_view_model;
}

void PrintPreviewViewModel::ImmediateRefresh()
{
    RequestRefresh();
}
void PrintPreviewViewModel::QueueRefresh()
{
    m_RefreshTimer.start();
}

bool PrintPreviewViewModel::HasBacksides() const
{
    return m_Project.m_Data.m_BacksideEnabled;
}

std::vector<Page> PrintPreviewViewModel::GetFrontsidePages() const
{
    return DistributeCardsToPages(m_Project);
}
std::vector<PageImageTransform> PrintPreviewViewModel::GetFrontsideTransforms() const
{
    return ComputeTransforms(m_Project, m_Cfg.m_NoCropMode);
}

std::vector<Page> PrintPreviewViewModel::GetBacksidePages(const std::vector<Page>& frontside_pages) const
{
    return MakeBacksidePages(m_Project, frontside_pages);
}
std::vector<PageImageTransform> PrintPreviewViewModel::GetBacksideTransforms(const std::vector<PageImageTransform>& frontside_transforms) const
{
    return ComputeBacksideTransforms(m_Project, frontside_transforms, m_Cfg.m_NoCropMode);
}

void PrintPreviewViewModel::RestoreCardsOrder()
{
    m_Project.RestoreCardsOrder();
}
void PrintPreviewViewModel::ReorderCards(size_t from, size_t to)
{
    m_Project.ReorderCards(from, to);
}

void PrintPreviewViewModel::RestoreAllSlots()
{
    m_Project.RestoreAllSlots();
}

void PrintPreviewViewModel::CardOrderChanged()
{
    if (!m_Project.IsManuallySorted())
    {
        m_RefreshTimer.start();
    }
}
void PrintPreviewViewModel::CardOrderDirectionChanged()
{
    if (!m_Project.IsManuallySorted())
    {
        m_RefreshTimer.start();
    }
}

void PrintPreviewViewModel::RenderSortingChanged()
{
    m_RefreshTimer.start();
}

void PrintPreviewViewModel::SkippedSlotsChanged(std::span<const size_t> skipped_slots)
{
    SlotsSkippedChanged(!skipped_slots.empty());
}

void PrintPreviewViewModel::EmitDefaults()
{
    CardsManuallySortedChanged(m_Project.IsManuallySorted());
    SlotsSkippedChanged(!m_Project.m_Data.m_SkippedLayoutSlots.empty());

    RenderPageBackground();
}

void PrintPreviewViewModel::PageSizeChanged()
{
    RenderPageBackground();
}
void PrintPreviewViewModel::BasePdfChanged()
{
    RenderPageBackground();
}
void PrintPreviewViewModel::UnderlayPdfChanged()
{
    RenderPageBackground();
}

void PrintPreviewViewModel::RenderPageBackground()
{
    m_PageBackground = QPixmap{};

    const auto base_pdf{ m_Project.GetBasePdfPath() };
    if (base_pdf.has_value())
    {
        if (const auto base_image{ RenderPdf(base_pdf.value()) })
        {
            m_PageBackground = StoreIntoQtPixmap(base_image);
        }
    }

    const auto underlay_pdf{ m_Project.GetUnderlayPdfPath() };
    if (underlay_pdf.has_value())
    {
        if (const auto underlay_image{ RenderPdf(underlay_pdf.value()) })
        {
            auto underlay_pixmap{ StoreIntoQtPixmap(underlay_image) };

            const auto page_size{ m_Project.ComputePageSize() };
            const auto pdf_size{ LoadPdfSize(underlay_pdf.value()).value() };
            if (m_PageBackground.isNull())
            {
                const auto pixel_density{ static_cast<float>(underlay_pixmap.width()) / pdf_size.x };
                const auto target_width_px{ qRound(page_size.x * pixel_density) };
                const auto target_height_px{ qRound(page_size.y * pixel_density) };

                m_PageBackground = QPixmap{ target_width_px, target_height_px };
                m_PageBackground.fill(Qt::transparent);
            }
            else
            {
                const auto scale_factor{ static_cast<float>(m_PageBackground.width()) / underlay_pixmap.width() };
                if (scale_factor > 1.0f)
                {
                    const auto target_width_px{ qRound(underlay_pixmap.width() * scale_factor) };
                    const auto target_height_px{ qRound(underlay_pixmap.height() * scale_factor) };
                    underlay_pixmap = underlay_pixmap.scaled(target_width_px, target_height_px);
                }
                else
                {
                    const auto target_width_px{ qRound(m_PageBackground.width() * scale_factor) };
                    const auto target_height_px{ qRound(m_PageBackground.height() * scale_factor) };
                    m_PageBackground = m_PageBackground.scaled(target_width_px, target_height_px);
                }
            }

            const auto x{ (m_PageBackground.width() - underlay_pixmap.width()) / 2 };
            const auto y{ (m_PageBackground.height() - underlay_pixmap.height()) / 2 };

            QPainter painter{ &m_PageBackground };
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawPixmap(x, y, underlay_pixmap);
            painter.end();
        }
    }

    PageBackgroundChanged(m_PageBackground);
}
