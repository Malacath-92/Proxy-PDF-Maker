#pragma once

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QSlider;

class DefaultBacksidePreview;
class Project;

class CardOptionsWidget : public QWidget
{
  public:
    CardOptionsWidget(Project& project);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BaseUnitChanged();

  private:
    Project& m_Project;

    QDoubleSpinBox* m_BleedEdgeSpin{ nullptr };
    QSlider* m_CornerWeightSlider{ nullptr };
    QCheckBox* m_BacksideCheckbox{ nullptr };
    QDoubleSpinBox* m_BacksideOffsetSpin{ nullptr };
    DefaultBacksidePreview* m_BacksideDefaultPreview{ nullptr };
};
