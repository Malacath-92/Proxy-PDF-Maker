#include <ppp/ui/view_models/options/view_model_guides_options.hpp>

#include <charconv>

#include <nlohmann/json.hpp>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/default_project_value_actions.hpp>

#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

GuidesOptionsViewModel::GuidesOptionsViewModel(Project& project,
                                               const Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Guides Options");
}

void GuidesOptionsViewModel::ChangeExportExactGuides(Qt::CheckState export_exact_guides)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetExportExactGuides(export_exact_guides != Qt::CheckState::Unchecked);
}
void GuidesOptionsViewModel::ChangeGuidesEnabled(Qt::CheckState guides_enabled)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetGuidesEnabled(guides_enabled != Qt::CheckState::Unchecked);
}
void GuidesOptionsViewModel::ChangeBacksideGuidesEnabled(Qt::CheckState backside_guides_enabled)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetBacksideGuidesEnabled(backside_guides_enabled != Qt::CheckState::Unchecked);
}
void GuidesOptionsViewModel::ChangeCornerGuidesEnabled(Qt::CheckState corner_guides_enabled)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetCornerGuidesEnabled(corner_guides_enabled != Qt::CheckState::Unchecked);
}
void GuidesOptionsViewModel::ChangeCrossGuidesEnabled(Qt::CheckState cross_guides_enabled)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetCrossGuidesEnabled(cross_guides_enabled != Qt::CheckState::Unchecked);
}
void GuidesOptionsViewModel::ChangeExtendedGuidesEnabled(Qt::CheckState extended_guides_enabled)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetExtendedGuidesEnabled(extended_guides_enabled != Qt::CheckState::Unchecked);
}
void GuidesOptionsViewModel::ChangeGuidesColorA(ColorRGB8 guides_color)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetGuidesColorA(guides_color);
}
void GuidesOptionsViewModel::ChangeGuidesColorB(ColorRGB8 guides_color)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetGuidesColorB(guides_color);
}
void GuidesOptionsViewModel::ChangeGuidesOffset(Length guides_offset)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetGuidesOffset(guides_offset);
}
void GuidesOptionsViewModel::ChangeGuidesLength(Length guides_length)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetGuidesLength(guides_length);
}
void GuidesOptionsViewModel::ChangeGuidesThickness(Length guides_thickness)
{
    TRACY_AUTO_SCOPE();

    m_Project.SetGuidesThickness(guides_thickness);
}

void GuidesOptionsViewModel::NewProjectOpened()
{
    EmitDefaults();
}

void GuidesOptionsViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    AdvancedModeChanged(m_Cfg.m_AdvancedMode);
    BaseUnitChanged(m_Cfg.m_BaseUnit);

    ExportExactGuidesChanged(m_Project.m_Data.m_ExportExactGuides);
    GuidesEnabledChanged(m_Project.m_Data.m_EnableGuides);
    BacksideGuidesEnabledChanged(m_Project.m_Data.m_BacksideEnableGuides);
    CornerGuidesEnabledChanged(m_Project.m_Data.m_CornerGuides);
    CrossGuidesEnabledChanged(m_Project.m_Data.m_CrossGuides);
    ExtendedGuidesEnabledChanged(m_Project.m_Data.m_ExtendedGuides);
    GuidesColorAChanged(m_Project.m_Data.m_GuidesColorA);
    GuidesColorBChanged(m_Project.m_Data.m_GuidesColorB);
    GuidesOffsetChanged(m_Project.m_Data.m_GuidesOffset);
    GuidesLengthChanged(m_Project.m_Data.m_GuidesLength);
    GuidesThicknessChanged(m_Project.m_Data.m_GuidesThickness);

    CardSizeChanged(m_Project.CardSize());
    BleedEdgeChanged(m_Project.m_Data.m_BleedEdge);
    BacksideEnabledChanged(m_Project.m_Data.m_BacksideEnabled);
}

DefaultDataRequirements GuidesOptionsViewModel::GetDefaultDataRequirements() const
{
    return DefaultDataRequirements{
        std::string{ m_Cfg.GetFirstValidCardSize() },
        std::string{ m_Cfg.GetFirstValidPageSize() },
    };
}
bool GuidesOptionsViewModel::GetAdvancedMode() const
{
    return m_Cfg.m_AdvancedMode;
}
Unit GuidesOptionsViewModel::GetBaseUnit() const
{
    return m_Cfg.m_BaseUnit;
}

bool GuidesOptionsViewModel::GetCornerGuidesEnabled() const
{
    return m_Project.m_Data.m_CornerGuides;
}
Length GuidesOptionsViewModel::GetBleedEdge() const
{
    return m_Project.m_Data.m_BleedEdge;
}
Length GuidesOptionsViewModel::GetEnvelopeBleedEdge() const
{
    return m_Project.m_Data.m_EnvelopeBleedEdge;
}

const Project& GuidesOptionsViewModel::GetProject() const
{
    return m_Project;
}