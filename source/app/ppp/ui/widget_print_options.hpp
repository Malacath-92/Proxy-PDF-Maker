#pragma once

#include <QWidget>
#include <QDoubleSpinBox>
#include <QWheelEvent>

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

    // Margin control system provides both simple and advanced layout options
    // The toggle between modes allows users to choose between quick uniform margins
    // and precise individual control for professional printing requirements
    QCheckBox* m_CustomMargins{ nullptr };
    QCheckBox* m_MarginModeToggle{ nullptr };

    // Individual margin controls enable asymmetric layouts needed for binding,
    // cutting guides, or when different margins are required for aesthetic reasons
    QDoubleSpinBox* m_LeftMarginSpin{ nullptr };
    QDoubleSpinBox* m_TopMarginSpin{ nullptr };
    QDoubleSpinBox* m_RightMarginSpin{ nullptr };
    QDoubleSpinBox* m_BottomMarginSpin{ nullptr };

    // All-margins control provides quick uniform margin adjustment for common scenarios
    // where symmetric margins are sufficient for the printing requirements
    QDoubleSpinBox* m_AllMarginsSpin{ nullptr };
};
