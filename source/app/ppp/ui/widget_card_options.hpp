#pragma once

#include <QWidget>

class QCheckBox;
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
    void BacksideEnabledChanged();
    void BacksideDefaultChanged();
    void BacksideOffsetChanged();

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BaseUnitChanged();

  private:
    void SetDefaults();

    Project& m_Project;

    QDoubleSpinBox* m_BleedEdgeSpin{ nullptr };
    QDoubleSpinBox* m_SpacingSpin{ nullptr };
    QCheckBox* m_BacksideCheckbox{ nullptr };
    QPushButton* m_BacksideDefaultButton{ nullptr };
    DefaultBacksidePreview* m_BacksideDefaultPreview{ nullptr };
    QDoubleSpinBox* m_BacksideOffsetSpin{ nullptr };
    QWidget* m_BacksideOffset{ nullptr };
};
