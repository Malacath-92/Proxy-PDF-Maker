#pragma once

#include <QString>
#include <QWidget>

#include <ppp/color.hpp>
#include <ppp/units.hpp>

class QCheckBox;

class Project;
struct Config;
class WidgetWithLabel;
class LengthSpinBox;

class GuidesOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GuidesOptionsWidget(Project& project,
                        const Config& config);

  signals:
    void ExactGuidesEnabledChanged();
    void GuidesEnabledChanged();
    void BacksideGuidesEnabledChanged();
    void CornerGuidesChanged();
    void CrossGuidesChanged();
    void GuidesOffsetChanged();
    void GuidesLengthChanged();
    void ExtendedGuidesChanged();
    void GuidesColorChanged();
    void GuidesThicknessChanged();

    void BaseUnitChanged(Unit new_base_unit);

  public slots:
    void NewProjectOpened();
    void CardSizeChanged();
    void BleedChanged();
    void BacksideEnabledChanged();

    void AdvancedModeChanged(bool advanced_mode);

  private:
    void SetDefaults();
    void SetAdvancedWidgetsVisibility(bool advanced_mode);

    static QString ColorToBackgroundStyle(ColorRGB8 color);

    Project& m_Project;

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
