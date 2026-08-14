#pragma once

#include <QString>
#include <QWidget>

#include <ppp/color.hpp>
#include <ppp/units.hpp>

class QCheckBox;

class GuidesOptionsViewModel;
class WidgetWithLabel;
class LengthSpinBox;

class GuidesOptionsWidget : public QWidget
{
    Q_OBJECT

    friend class GuidesOptionsViewModel;

  public:
    GuidesOptionsWidget(GuidesOptionsViewModel* view_model);

  signals:
    // forward

    void BaseUnitChanged(Unit base_unit);

  private slots:
    void AdvancedModeChanged(bool advanced_mode);

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

  private:
    static ColorRGB8 ColorFromBackgroundStyle(const QString& style);
    static QString ColorToBackgroundStyle(ColorRGB8 color);

    GuidesOptionsViewModel& m_ViewModel;

    QCheckBox* m_ExportExactGuidesCheckbox{ nullptr };
    QCheckBox* m_EnableGuidesCheckbox{ nullptr };
    QCheckBox* m_EnableBacksideGuidesCheckbox{ nullptr };
    QCheckBox* m_CornerGuidesCheckbox{ nullptr };
    QCheckBox* m_CrossGuidesCheckbox{ nullptr };
    LengthSpinBox* m_GuidesOffsetSpin{ nullptr };
    LengthSpinBox* m_GuidesLengthSpin{ nullptr };
    QCheckBox* m_ExtendedGuidesCheckbox{ nullptr };
    WidgetWithLabel* m_GuidesColorA{ nullptr };
    WidgetWithLabel* m_GuidesColorB{ nullptr };
    LengthSpinBox* m_GuidesThicknessSpin{ nullptr };
};
