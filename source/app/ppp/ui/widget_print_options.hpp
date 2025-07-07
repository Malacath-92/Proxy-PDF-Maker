#pragma once

#include <QWidget>

#include <ppp/util.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

class Project;
class ComboBoxWithLabel;

class PrintOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    PrintOptionsWidget(Project& project);

  signals:
    void PageSizeChanged();
    void CardSizeChanged();
    void MarginsChanged();
    void CardLayoutChanged();
    void OrientationChanged();
    void FlipOnChanged();

  public slots:
    void NewProjectOpened();
    void BleedChanged();
    void SpacingChanged();
    void BaseUnitChanged();
    void RenderBackendChanged();

  private:
    void SetDefaults();

    void RefreshSizes();
    void RefreshCardLayout();

    static std::vector<std::string> GetBasePdfNames();
    static std::string SizeToString(Size size);

    Project& m_Project;

    QLineEdit* m_PrintOutput{ nullptr };
    QComboBox* m_CardSize{ nullptr };
    QComboBox* m_PaperSize{ nullptr };
    QDoubleSpinBox* m_CardsWidth{ nullptr };
    QDoubleSpinBox* m_CardsHeight{ nullptr };
    QComboBox* m_Orientation{ nullptr };
    QComboBox* m_FlipOn{ nullptr };
    QLabel* m_PaperInfo{ nullptr };
    QLabel* m_CardsInfo{ nullptr };
    ComboBoxWithLabel* m_BasePdf{ nullptr };
    QCheckBox* m_CustomMargins{ nullptr };
    QDoubleSpinBox* m_LeftMarginSpin{ nullptr };
    QDoubleSpinBox* m_TopMarginSpin{ nullptr };
};
