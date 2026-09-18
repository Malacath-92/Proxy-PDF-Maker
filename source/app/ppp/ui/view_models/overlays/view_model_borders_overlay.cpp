#include <ppp/ui/view_models/overlays/view_model_borders_overlay.hpp>

#include <ppp/project/project.hpp>

#include <ppp/pdf/util.hpp>
#include <ppp/svg/util.hpp>

BordersOverlayViewModel::BordersOverlayViewModel(const Project& project,
                                                 bool is_backside)
    : m_Project{ project }
    , m_IsBackside{ is_backside }
{
    QObject::connect(&project, &Project::ExportExactGuidesChanged, this, &BordersOverlayViewModel::Redraw);
}

bool BordersOverlayViewModel::ShouldDrawBorders() const
{
    return m_Project.m_Data.m_ExportExactGuides;
}
bool BordersOverlayViewModel::ShouldDrawOuterBorder() const
{
    return m_Project.m_Data.m_BleedEdge > 0_mm ||
           m_Project.m_Data.m_EnvelopeBleedEdge > 0_mm;
}
bool BordersOverlayViewModel::ShouldDrawRoundedRect() const
{
    return m_Project.IsCardRoundedRect();
}
bool BordersOverlayViewModel::ShouldDrawCardSvg() const
{
    return m_Project.IsCardSvg();
}

Size BordersOverlayViewModel::GetPageSize() const
{
    return m_Project.ComputePageSize();
}
Size BordersOverlayViewModel::GetCardsSize() const
{
    return m_Project.ComputeCardsSize();
}
Length BordersOverlayViewModel::GetCardCornerRadius() const
{
    return m_Project.CardCornerRadius();
}
Margins BordersOverlayViewModel::GetPageMargins() const
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

void BordersOverlayViewModel::DrawCardSvg(QPainterPath& painter_path,
                                          const PageImageTransform& transform,
                                          Size size) const
{
    DrawSvgToPainterPath(painter_path,
                         m_Project.CardSvgData(),
                         transform.m_Card.m_Position,
                         transform.m_Card.m_Size,
                         false,
                         m_IsBackside,
                         transform.m_Rotation,
                         size);
}
