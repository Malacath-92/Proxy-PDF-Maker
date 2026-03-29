#pragma once

#include <QDoubleSpinBox>
#include <QWheelEvent>
#include <QWidget>

#include <ppp/util.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

class Project;
class WidgetWithLabel;
class ComboBoxWithLabel;
class LengthSpinBox;

class PrintOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    PrintOptionsWidget(Project& project);

  signals:
    void PageSizeChanged();
    void PageSizesChanged();
    void CardSizeChanged();
    void CardSizesChanged();
    void MarginsChanged();
    void CardOrientationChanged();
    void CardLayoutChanged();
    void OrientationChanged();
    void FlipOnChanged();

    void BaseUnitChanged();

  public slots:
    void NewProjectOpened();
    void BleedChanged();
    void SpacingChanged();

    void AdvancedModeChanged();
    void RenderBackendChanged();

    void BasePdfAdded();

    void ExternalCardSizeChanged();
    void ExternalCardSizesChanged();

  private:
    void SetDefaults();
    void SetAdvancedWidgetsVisibility();

    void RefreshSizes();
    void RefreshMargins(bool reset_margins);
    void RefreshCardLayout();

    static std::vector<std::string> GetBasePdfNames();
    static std::string SizeToString(Size size);

    Project& m_Project;

    QLineEdit* m_PrintOutput{ nullptr };
    QCheckBox* m_RenderHeader{ nullptr };
    QComboBox* m_CardSize{ nullptr };
    QComboBox* m_PaperSize{ nullptr };
    ComboBoxWithLabel* m_BasePdf{ nullptr };
    QComboBox* m_CardOrientation{ nullptr };
    WidgetWithLabel* m_CardsLayoutVertical{ nullptr };
    QDoubleSpinBox* m_CardsWidthVertical{ nullptr };
    QDoubleSpinBox* m_CardsHeightVertical{ nullptr };
    WidgetWithLabel* m_CardsLayoutHorizontal{ nullptr };
    QDoubleSpinBox* m_CardsWidthHorizontal{ nullptr };
    QDoubleSpinBox* m_CardsHeightHorizontal{ nullptr };
    ComboBoxWithLabel* m_Orientation{ nullptr };
    QComboBox* m_FlipOn{ nullptr };
    QLabel* m_PaperInfo{ nullptr };
    QLabel* m_CardsInfo{ nullptr };

    // Margin control system provides both simple and advanced layout options
    // The toggle between modes allows users to choose between quick uniform margins
    // and precise individual control for professional printing requirements
    QComboBox* m_MarginsMode{ nullptr };

    // Individual margin controls enable asymmetric layouts needed for binding,
    // cutting guides, or when different margins are required for aesthetic reasons
    LengthSpinBox* m_LeftMarginSpin{ nullptr };
    LengthSpinBox* m_TopMarginSpin{ nullptr };
    LengthSpinBox* m_RightMarginSpin{ nullptr };
    LengthSpinBox* m_BottomMarginSpin{ nullptr };

    // All-margins control provides quick uniform margin adjustment for common scenarios
    // where symmetric margins are sufficient for the printing requirements
    LengthSpinBox* m_AllMarginsSpin{ nullptr };
};
