#include <ppp/ui/view_models/overlays/view_model_guides_overlay.hpp>

#include <ppp/project/project.hpp>

#include <ppp/pdf/util.hpp>
#include <ppp/svg/util.hpp>

GuidesOverlayViewModel::GuidesOverlayViewModel(const Project& project)
    : m_Project{ project }
{
    QObject::connect(&project, &Project::GuidesEnabledChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::BacksideGuidesEnabledChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::CornerGuidesEnabledChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::CrossGuidesEnabledChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::ExtendedGuidesEnabledChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::GuidesColorAChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::GuidesColorBChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::GuidesOffsetChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::GuidesLengthChanged, this, &GuidesOverlayViewModel::Redraw);
    QObject::connect(&project, &Project::GuidesThicknessChanged, this, &GuidesOverlayViewModel::Redraw);

    QObject::connect(&project, &Project::GuidesColorAChanged, this, &GuidesOverlayViewModel::SomeGuidesColorChanged);
    QObject::connect(&project, &Project::GuidesColorBChanged, this, &GuidesOverlayViewModel::SomeGuidesColorChanged);
}

bool GuidesOverlayViewModel::ShouldDrawGuides() const
{
    return m_Project.m_Data.m_EnableGuides;
}
bool GuidesOverlayViewModel::ShouldDrawCornerGuides() const
{
    return m_Project.m_Data.m_CornerGuides;
}
bool GuidesOverlayViewModel::ShouldDrawCrossGuides() const
{
    return m_Project.m_Data.m_CrossGuides;
}
bool GuidesOverlayViewModel::ShouldDrawExtendedGuides() const
{
    return m_Project.m_Data.m_ExtendedGuides;
}

Size GuidesOverlayViewModel::GetPageSize() const
{
    return m_Project.ComputePageSize();
}
Length GuidesOverlayViewModel::GetGuidesThickness() const
{
    return m_Project.m_Data.m_GuidesThickness;
}
Length GuidesOverlayViewModel::GetGuidesLength() const
{
    return m_Project.m_Data.m_GuidesLength;
}
Length GuidesOverlayViewModel::GetGuidesOffset() const
{
    return m_Project.m_Data.m_GuidesOffset;
}
Length GuidesOverlayViewModel::GetBleedEdge() const
{
    return m_Project.m_Data.m_BleedEdge;
}
Length GuidesOverlayViewModel::GetEnvelopeBleedEdge() const
{
    return m_Project.m_Data.m_EnvelopeBleedEdge;
}

void GuidesOverlayViewModel::EmitDefaults()
{
    SomeGuidesColorChanged();
}

void GuidesOverlayViewModel::SomeGuidesColorChanged()
{
    GuidesColorsChanged(m_Project.m_Data.m_GuidesColorA,
                        m_Project.m_Data.m_GuidesColorB);
}
