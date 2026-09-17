#include <ppp/ui/view_models/view_model_print_preview.hpp>

#include <ppp/config.hpp>
#include <ppp/project/project.hpp>

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
    return new PagePreviewViewModel{ m_Project, m_Cfg, is_backside };
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
}
