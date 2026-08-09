#pragma once

#include <QObject>

#include <ppp/project/project_types.hpp>
#include <ppp/units.hpp>

class Project;
class Config;

struct DefaultDataRequirements;

class CardOptionsViewModel : public QObject
{
    Q_OBJECT

    friend class CardOptionsWidget;

  public:
    CardOptionsViewModel(Project& project,
                         const Config& config);

  signals:
    // forward

    void AdvancedModeChanged(bool advanced_mode);
    void BaseUnitChanged(Unit base_unit);

    void BacksideEnabledChanged(bool backside_enabled);
    void SeparateBacksidesEnabledChanged(bool separate_backsides);

    void BacksideDefaultChanged(OptionalImageRef backside_card_name);

    void BacksideOffsetChanged(Size offset);
    void BacksideRotationChanged(Angle backside_rotation);
    void BacksideExtraBleedEdgeChanged(Length backside_extra_bleed_edge);

    void BacksideAutoPatternChanged(const std::string& pattern);

    void BleedEdgeChanged(Length bleed_edge);
    void EnvelopeBleedEdgeChanged(Length envelope_bleed_edge);

    void SpacingChanged(Size spacing);
    void SpacingLinkedChanged(bool spacing_linked);

    void CornersChanged(CardCorners corners);

    void ImageDirChanged(const fs::path& old_path, const fs::path& new_path);

  public slots:
    void NewProjectOpened();

  private slots:
    void ChangeBacksideEnabled(Qt::CheckState backside_enabled);
    void ChangeSeparateBacksidesEnabled(Qt::CheckState separate_backsides);

    void ChangeBacksideDefault(const QString& backside_card_name);
    void ClearBacksideDefault();

    void ChangeBacksideOffset(Size offset);
    void ChangeBacksideRotation(double backside_rotation);
    void ChangeBacksideExtraBleedEdge(Length backside_extra_bleed_edge);

    void ChangeBacksideAutoPattern(const QString& pattern);

    void ChangeBleedEdge(Length bleed_edge);
    void ChangeEnvelopeBleedEdge(Length envelope_bleed_edge);

    void ChangeSpacing(Size spacing);
    void ChangeSpacingLinked(bool spacing_linked);

    void ChangeCorners(const QString& corners);

  private:
    void EmitDefaults();

    DefaultDataRequirements GetDefaultDataRequirements() const;
    bool GetAdvancedMode() const;
    Unit GetBaseUnit() const;

    OptionalImageRef GetBacksideDefault() const;

    Length GetFullBleed() const;
    Length GetTotalBleed() const;

    // TODO: KillMe
    const Project& GetProject() const;

    Project& m_Project;
    const Config& m_Cfg;
};
