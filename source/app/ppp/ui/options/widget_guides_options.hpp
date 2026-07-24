#pragma once

#include <QString>
#include <QWidget>

#include <ppp/color.hpp>

class QCheckBox;

class Project;
class WidgetWithLabel;
class LengthSpinBox;

class GuidesOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GuidesOptionsWidget(Project& project);

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

    void BaseUnitChanged();

  public slots:
    void NewProjectOpened();
    void CardSizeChanged();
    void BleedChanged();
    void BacksideEnabledChanged();

    void AdvancedModeChanged();

  private:
    void SetDefaults();
    void SetAdvancedWidgetsVisibility();

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
