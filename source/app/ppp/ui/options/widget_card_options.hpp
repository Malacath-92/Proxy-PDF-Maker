#pragma once

#include <QWidget>

#include <ppp/project/project_types.hpp>
#include <ppp/units.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QLineEdit;

struct DefaultDataRequirements;
class DefaultBacksidePreview;
class CardOptionsViewModel;
class LengthSpinBox;
class LinkedSpinBoxes;

class CardOptionsWidget : public QWidget
{
    Q_OBJECT

    friend class CardOptionsViewModel;

  public:
    CardOptionsWidget(CardOptionsViewModel* view_model);

  signals:
    // forward

    void BaseUnitChanged(Unit base_unit);

  private slots:
    void AdvancedModeChanged(bool advanced_mode);

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

  private:
    CardOptionsViewModel& m_ViewModel;

    LengthSpinBox* m_BleedEdgeSpin{ nullptr };
    LengthSpinBox* m_EnvelopeSpin{ nullptr };
    LengthSpinBox* m_HorizontalSpacingSpin{ nullptr };
    LengthSpinBox* m_VerticalSpacingSpin{ nullptr };
    LinkedSpinBoxes* m_Spacing{ nullptr };
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
