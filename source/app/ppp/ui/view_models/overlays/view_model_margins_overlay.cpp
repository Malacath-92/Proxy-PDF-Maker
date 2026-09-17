#include <ppp/ui/view_models/overlays/view_model_margins_overlay.hpp>

#include <ppp/project/project.hpp>

MarginsOverlayViewModel::MarginsOverlayViewModel(const Project& project,
                                                 bool is_backside)
    : m_Project{ project }
    , m_IsBackside{ is_backside }
{
}

Size MarginsOverlayViewModel::GetPageSize() const
{
    return m_Project.ComputePageSize();
}
Margins MarginsOverlayViewModel::GetPageMargins() const
{
    auto margins{ m_Project.ComputeMargins() };
    if (m_IsBackside)
    {
        if (m_Project.m_Data.m_FlipOn == FlipPageOn::LeftEdge)
        {
            std::swap(margins.m_Left, margins.m_Right);
        }
        else
        {
            std::swap(margins.m_Top, margins.m_Bottom);
        }

        const auto backside_offset{ m_Project.m_Data.m_BacksideOffset };
        margins.m_Left -= backside_offset.x;
        margins.m_Right += backside_offset.x;
        margins.m_Top -= backside_offset.y;
        margins.m_Bottom += backside_offset.y;
    }
    return margins;
}