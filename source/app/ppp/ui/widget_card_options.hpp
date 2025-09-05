#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSlider;

class DefaultBacksidePreview;
class Project;

class CardOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    CardOptionsWidget(Project& project);

  signals:
    void BleedChanged();
    void SpacingChanged();
    void CornersChanged();
    void BacksideEnabledChanged();
    void BacksideDefaultChanged();
    void BacksideOffsetChanged();

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BaseUnitChanged();

    void AdvancedModeChanged();

    void BacksideEnabledChangedExternal();

  private:
    void SetDefaults();

    Project& m_Project;

    QDoubleSpinBox* m_BleedEdgeSpin{ nullptr };
    QDoubleSpinBox* m_HorizontalSpacingSpin{ nullptr };
    QDoubleSpinBox* m_VerticalSpacingSpin{ nullptr };
    QComboBox* m_Corners{ nullptr };
    QCheckBox* m_BacksideCheckbox{ nullptr };
    QPushButton* m_BacksideDefaultButton{ nullptr };
    DefaultBacksidePreview* m_BacksideDefaultPreview{ nullptr };
    QDoubleSpinBox* m_BacksideOffsetSpin{ nullptr };
    QWidget* m_BacksideOffset{ nullptr };
};
