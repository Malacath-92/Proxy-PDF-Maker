#pragma once

#include <QWidget>

#include <ppp/units.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QLineEdit;

class DefaultBacksidePreview;
class Project;
struct Config;
class LengthSpinBox;

class CardOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    CardOptionsWidget(Project& project,
                      const Config& config);

  signals:
    void BleedChanged();
    void EnvelopeBleedChanged();
    void SpacingChanged();
    void CornersChanged();
    void BacksideEnabledChanged();
    void SeparateBacksidesEnabledChanged();
    void BacksideDefaultChanged();
    void BacksideOffsetChanged();
    void BacksideExtraBleedChanged();
    void BacksideRotationChanged();
    void CardBacksideChanged();

    void BaseUnitChanged(Unit base_unit);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();

    void AdvancedModeChanged();

    void BacksideEnabledChangedExternal();
    void BacksideAutoPatternChangedExternal(const std::string& pattern);

  private:
    void SetDefaults();
    void SetAdvancedWidgetsVisibility();
    void SetBacksideAutoPatternTooltip();

    Project& m_Project;
    const Config& m_Cfg;

    LengthSpinBox* m_BleedEdgeSpin{ nullptr };
    LengthSpinBox* m_EnvelopeSpin{ nullptr };
    LengthSpinBox* m_HorizontalSpacingSpin{ nullptr };
    LengthSpinBox* m_VerticalSpacingSpin{ nullptr };
    QComboBox* m_Corners{ nullptr };
    QCheckBox* m_BacksideCheckbox{ nullptr };
    QCheckBox* m_SeparateBacksidesCheckbox{ nullptr };
    QPushButton* m_BacksideDefaultButton{ nullptr };
    DefaultBacksidePreview* m_BacksideDefaultPreview{ nullptr };
    LengthSpinBox* m_BacksideOffsetHorizontalSpin{ nullptr };
    LengthSpinBox* m_BacksideOffsetVerticalSpin{ nullptr };
    QWidget* m_BacksideOffset{ nullptr };
    LengthSpinBox* m_BacksideExtraBleedEdgeSpin{ nullptr };
    QWidget* m_BacksideExtraBleedEdge{ nullptr };
    QDoubleSpinBox* m_BacksideRotationSpin{ nullptr };
    QWidget* m_BacksideRotation{ nullptr };
    QLineEdit* m_BacksideAutoPattern{ nullptr };
    QWidget* m_BacksideAuto{ nullptr };
};
