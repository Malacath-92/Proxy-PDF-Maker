#include <ppp/ui/view_models/view_model_page_preview.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/view_models/overlays/view_model_borders_overlay.hpp>
#include <ppp/ui/view_models/overlays/view_model_guides_overlay.hpp>
#include <ppp/ui/view_models/overlays/view_model_margins_overlay.hpp>
#include <ppp/ui/view_models/view_model_card.hpp>

PagePreviewViewModel::PagePreviewViewModel(Project& project,
                                           const Config& config,
                                           bool is_backside)
    : m_Project{ project }
    , m_Cfg{ config }
    , m_IsBackside{ is_backside }
{
}

CardViewModel* PagePreviewViewModel::MakeCardViewModel(const fs::path& card_name,
                                                       Rotation rotation) const
{
    return new CardViewModel{
        card_name,
        CardViewParams{
            .m_RoundedCorners = HasRoundedCorners(),
            .m_Backside = m_IsBackside,
            .m_Rotation = rotation,
            .m_BleedEdge{ GetBleedEdge() },
        },
        m_Project,
    };
}

GuidesOverlayViewModel* PagePreviewViewModel::MakeGuidesOverlayViewModel() const
{
    return new GuidesOverlayViewModel{ m_Project };
}
BordersOverlayViewModel* PagePreviewViewModel::MakeBordersOverlayViewModel(bool is_backside) const
{
    return new BordersOverlayViewModel{ m_Project, is_backside };
}
MarginsOverlayViewModel* PagePreviewViewModel::MakeMarginsOverlayViewModel(bool is_backside) const
{
    return new MarginsOverlayViewModel{ m_Project, is_backside };
}

Size PagePreviewViewModel::GetPageSize() const
{
    return m_Project.ComputePageSize();
}
Length PagePreviewViewModel::GetBleedEdge() const
{
    if (m_Cfg.m_NoCropMode)
    {
        return m_Project.CardFullBleed();
    }

    const auto total_bleed_edge{
        m_IsBackside
            ? m_Project.m_Data.m_BleedEdge +
                  m_Project.m_Data.m_EnvelopeBleedEdge +
                  m_Project.m_Data.m_BacksideExtraBleedEdge
            : m_Project.m_Data.m_BleedEdge +
                  m_Project.m_Data.m_EnvelopeBleedEdge
    };
    return total_bleed_edge;
}
bool PagePreviewViewModel::HasRoundedCorners() const
{
    if (m_Project.m_Data.m_Corners != CardCorners::Rounded)
    {
        return false;
    }

    const auto total_bleed_edge{
        m_IsBackside
            ? m_Project.m_Data.m_BleedEdge +
                  m_Project.m_Data.m_EnvelopeBleedEdge +
                  m_Project.m_Data.m_BacksideExtraBleedEdge
            : m_Project.m_Data.m_BleedEdge +
                  m_Project.m_Data.m_EnvelopeBleedEdge
    };
    return total_bleed_edge == 0_mm;
}
bool PagePreviewViewModel::IsBackside() const
{
    return m_IsBackside;
}

void PagePreviewViewModel::ReorderCards(size_t from, size_t to)
{
    m_Project.ReorderCards(from, to);
}
void PagePreviewViewModel::SkipSlot(size_t slot)
{
    m_Project.SkipSlot(slot);
}
