#include <ppp/ui/view_models/options/view_model_card_options.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ppp/config.hpp>

#include <ppp/ui/default_project_value_actions.hpp>

#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

CardOptionsViewModel::CardOptionsViewModel(Project& project,
                                           const Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Card Options");
}

void CardOptionsViewModel::NewProjectOpened()
{
    EmitDefaults();
}

void CardOptionsViewModel::ChangeBacksideEnabled(Qt::CheckState backside_enabled)
{
    m_Project.SetBacksideEnabled(backside_enabled != Qt::CheckState::Unchecked);
}
void CardOptionsViewModel::ChangeSeparateBacksidesEnabled(Qt::CheckState separate_backsides)
{
    m_Project.SetSeparateBacksidesEnabled(separate_backsides != Qt::CheckState::Unchecked);
}

void CardOptionsViewModel::ChangeBacksideDefault(const QString& backside_card_name)
{
    m_Project.SetBacksideDefault(backside_card_name.toStdString());
}
void CardOptionsViewModel::ClearBacksideDefault()
{
    m_Project.ClearBacksideDefault();
}
void CardOptionsViewModel::ChangeBacksideOffset(Size offset)
{
    m_Project.SetBacksideOffset(offset);
}
void CardOptionsViewModel::ChangeBacksideRotation(double backside_rotation)
{
    m_Project.SetBacksideRotation(static_cast<float>(backside_rotation) * 1_deg);
}
void CardOptionsViewModel::ChangeBacksideExtraBleedEdge(Length backside_extra_bleed_edge)
{
    m_Project.SetBacksideExtraBleedEdge(backside_extra_bleed_edge);
}

void CardOptionsViewModel::ChangeBacksideAutoPattern(const QString& pattern)
{
    m_Project.SetBacksideAutoPattern(pattern.toStdString());
}

void CardOptionsViewModel::ChangeBleedEdge(Length bleed_edge)
{
    m_Project.SetBleedEdge(bleed_edge);
}
void CardOptionsViewModel::ChangeEnvelopeBleedEdge(Length envelope_bleed_edge)
{
    m_Project.SetEnvelopeBleedEdge(envelope_bleed_edge);
}

void CardOptionsViewModel::ChangeSpacing(Size spacing)
{
    m_Project.SetSpacing(spacing);
}
void CardOptionsViewModel::ChangeSpacingLinked(bool spacing_linked)
{
    m_Project.SetSpacingLinked(spacing_linked);
}

void CardOptionsViewModel::ChangeCorners(const QString& corners)
{
    m_Project.SetCorners(magic_enum::enum_cast<CardCorners>(corners.toStdString())
                             .value_or(CardCorners::Square));
}

void CardOptionsViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    AdvancedModeChanged(m_Cfg.m_AdvancedMode);
    BaseUnitChanged(m_Cfg.m_BaseUnit);

    BacksideEnabledChanged(m_Project.m_Data.m_BacksideEnabled);
    SeparateBacksidesEnabledChanged(m_Project.m_Data.m_SeparateBacksides);

    BacksideDefaultChanged(m_Project.m_Data.m_BacksideDefault);

    BacksideOffsetChanged(m_Project.m_Data.m_BacksideOffset);
    BacksideRotationChanged(m_Project.m_Data.m_BacksideRotation);
    BacksideExtraBleedEdgeChanged(m_Project.m_Data.m_BacksideExtraBleedEdge);

    BacksideAutoPatternChanged(m_Project.m_Data.m_BacksideAutoPattern);

    BleedEdgeChanged(m_Project.m_Data.m_BleedEdge);
    EnvelopeBleedEdgeChanged(m_Project.m_Data.m_EnvelopeBleedEdge);

    SpacingChanged(m_Project.m_Data.m_Spacing);
    SpacingLinkedChanged(m_Project.m_Data.m_SpacingLinked);

    CornersChanged(m_Project.m_Data.m_Corners);
}

DefaultDataRequirements CardOptionsViewModel::GetDefaultDataRequirements() const
{
    return DefaultDataRequirements{
        std::string{ m_Cfg.GetFirstValidCardSize() },
        std::string{ m_Cfg.GetFirstValidPageSize() },
    };
}
bool CardOptionsViewModel::GetAdvancedMode() const
{
    return m_Cfg.m_AdvancedMode;
}
Unit CardOptionsViewModel::GetBaseUnit() const
{
    return m_Cfg.m_BaseUnit;
}

OptionalImageRef CardOptionsViewModel::GetBacksideDefault() const
{
    return m_Project.m_Data.m_BacksideDefault;
}

Length CardOptionsViewModel::GetFullBleed() const
{
    return m_Project.CardFullBleed();
}
Length CardOptionsViewModel::GetTotalBleed() const
{
    return m_Project.m_Data.m_BleedEdge +
           m_Project.m_Data.m_EnvelopeBleedEdge;
}

const Project& CardOptionsViewModel::GetProject() const
{
    return m_Project;
}
