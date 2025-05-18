#pragma once

#include <QString>
#include <QWidget>

#include <ppp/color.hpp>

class QCheckBox;
class QDoubleSpinBox;

class Project;
class WidgetWithLabel;

class GuidesOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GuidesOptionsWidget(Project& project);

  signals:
    void ExactGuidesEnabledChanged();
    void GuidesEnabledChanged();
    void BacksideGuidesEnabledChanged();
    void GuidesColorChanged();
    void GuidesOffsetChanged();
    void GuidesThicknessChanged();

  public slots:
    void NewProjectOpened();
    void BleedChanged();
    void BacksideEnabledChanged();
    void BaseUnitChanged();

  private:
    void SetDefaults();

    static QString ColorToBackgroundStyle(ColorRGB8 color);

    Project& m_Project;

    QCheckBox* m_ExportExactGuidesCheckbox{ nullptr };
    QCheckBox* m_EnableGuidesCheckbox{ nullptr };
    QCheckBox* m_BacksideGuidesCheckbox{ nullptr };
    QCheckBox* m_ExtendedGuidesCheckbox{ nullptr };
    WidgetWithLabel* m_GuidesColorA{ nullptr };
    WidgetWithLabel* m_GuidesColorB{ nullptr };
    QDoubleSpinBox* m_GuidesOffsetSpin{ nullptr };
    QDoubleSpinBox* m_GuidesThicknessSpin{ nullptr };
};
