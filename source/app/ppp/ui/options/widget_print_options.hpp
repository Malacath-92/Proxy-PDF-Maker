#pragma once

#include <QDoubleSpinBox>
#include <QWheelEvent>
#include <QWidget>

#include <ppp/config_types.hpp>
#include <ppp/units.hpp>

#include <ppp/project/project_types.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

class PrintOptionsViewModel;
class WidgetWithLabel;
class ComboBoxWithLabel;
class LengthSpinBox;

class PrintOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    PrintOptionsWidget(PrintOptionsViewModel* view_model);

  signals:
    // forward
    void BaseUnitChanged(Unit base_unit);

  private slots:
    void AdvancedModeChanged(bool advanced_mode);
    void AvailableCardSizesChanged(const CardSizes& card_sizes);
    void AvailablePageSizesChanged(const PageSizes& page_sizes);
    void AvailableBasePdfsChanged(std::span<const std::string> base_pdfs);

    void OutputFilenameChanged(const fs::path& output_filename);
    void PageHeaderEnabledChanged(bool page_header_enabled);
    void CardSizeChoiceChanged(std::string_view card_size_choice);
    void PageSizeChanged(Size page_size);
    void PageSizeChoiceChanged(std::string_view page_size_choice);
    void BasePdfChanged(std::string_view base_pdf);
    void CardsSizeChanged(Size cards_size);
    void PageMarginsModeChanged(MarginsMode margins_mode);
    void PageMarginsChanged(Margins margins);
    void MaxPageMarginsChanged(Size max_margins);
    void CardOrientationChanged(CardOrientation card_orientation);
    void CardsLayoutVerticalChanged(dla::uvec2 card_layout);
    void CardsLayoutHorizontalChanged(dla::uvec2 card_layout);
    void PageOrientationChanged(PageOrientation page_orientation);
    void FlipPageOnChanged(FlipPageOn flip_on);

  private:
    void OpenCardSizesPopup();
    void OpenPageSizesPopup();

    static std::string SizeToString(Size size, Unit unit);

    PrintOptionsViewModel& m_ViewModel;

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
