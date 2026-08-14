#pragma once

#include <QObject>
#include <QString>

#include <ppp/color.hpp>
#include <ppp/units.hpp>

class Project;
class Config;

struct DefaultDataRequirements;

class GuidesOptionsViewModel : public QObject
{
    Q_OBJECT

    friend class GuidesOptionsWidget;

  public:
    GuidesOptionsViewModel(Project& project,
                           const Config& config);

  signals:
    // forward

    void AdvancedModeChanged(bool advanced_mode);
    void BaseUnitChanged(Unit new_base_unit);

    void ExportExactGuidesChanged(bool export_exact_guides);
    void GuidesEnabledChanged(bool guides_enabled);
    void BacksideGuidesEnabledChanged(bool backside_guides_enabled);
    void CornerGuidesEnabledChanged(bool corner_guides_enabled);
    void CrossGuidesEnabledChanged(bool cross_guides_enabled);
    void ExtendedGuidesEnabledChanged(bool extended_guides_enabled);
    void GuidesColorAChanged(ColorRGB8 guides_color);
    void GuidesColorBChanged(ColorRGB8 guides_color);
    void GuidesOffsetChanged(Length guides_offset);
    void GuidesLengthChanged(Length guides_length);
    void GuidesThicknessChanged(Length guides_thickness);

    void CardSizeChanged(Size card_size);
    void BleedEdgeChanged(Length bleed_edge);
    void BacksideEnabledChanged(bool backside_enabled);

  public slots:
    void NewProjectOpened();

  private slots:
    void ChangeExportExactGuides(Qt::CheckState export_exact_guides);
    void ChangeGuidesEnabled(Qt::CheckState guides_enabled);
    void ChangeBacksideGuidesEnabled(Qt::CheckState backside_guides_enabled);
    void ChangeCornerGuidesEnabled(Qt::CheckState corner_guides_enabled);
    void ChangeCrossGuidesEnabled(Qt::CheckState cross_guides_enabled);
    void ChangeExtendedGuidesEnabled(Qt::CheckState extended_guides_enabled);
    void ChangeGuidesColorA(ColorRGB8 guides_color);
    void ChangeGuidesColorB(ColorRGB8 guides_color);
    void ChangeGuidesOffset(Length guides_offset);
    void ChangeGuidesLength(Length guides_length);
    void ChangeGuidesThickness(Length guides_thickness);

  private:
    void EmitDefaults();

    DefaultDataRequirements GetDefaultDataRequirements() const;
    bool GetAdvancedMode() const;
    Unit GetBaseUnit() const;

    bool GetCornerGuidesEnabled() const;
    Length GetBleedEdge() const;
    Length GetEnvelopeBleedEdge() const;

    // TODO: KillMe
    const Project& GetProject() const;

    Project& m_Project;
    const Config& m_Cfg;
};
