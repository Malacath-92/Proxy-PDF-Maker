#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QLineEdit;

class DefaultBacksidePreview;
class Project;
class LengthSpinBox;

class CardOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    CardOptionsWidget(Project& project);

  signals:
    void BleedChanged();
    void EnvelopeBleedChanged();
    void SpacingChanged();
    void CornersChanged();
    void BacksideEnabledChanged();
    void SeparateBacksidesEnabledChanged();
    void BacksideDefaultChanged();
    void BacksideOffsetChanged();
    void BacksideRotationChanged();
    void CardBacksideChanged();

    void BaseUnitChanged();

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
    QDoubleSpinBox* m_BacksideRotationSpin{ nullptr };
    QWidget* m_BacksideRotation{ nullptr };
    QLineEdit* m_BacksideAutoPattern{ nullptr };
    QWidget* m_BacksideAuto{ nullptr };
};
