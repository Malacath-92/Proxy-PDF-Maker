#include <ppp/ui/options/widget_print_options.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>

#include <nlohmann/json.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config_types.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/default_project_value_actions.hpp>
#include <ppp/ui/main_window.hpp>
#include <ppp/ui/widget_util/widget_combo_box.hpp>
#include <ppp/ui/widget_util/widget_double_spin_box.hpp>
#include <ppp/ui/widget_util/widget_label.hpp>

#include <ppp/ui/popups/card_size_popup.hpp>
#include <ppp/ui/popups/paper_size_popup.hpp>

#include <ppp/ui/view_models/options/view_model_print_options.hpp>
#include <ppp/ui/view_models/util.hpp>

#include <ppp/profile/profile.hpp>

PrintOptionsWidget::PrintOptionsWidget(PrintOptionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Print Options");
    m_ViewModel.setParent(this);

    const auto config_reqs{ m_ViewModel.GetDefaultDataRequirements() };
    const auto base_unit{ m_ViewModel.GetBaseUnit() };

    using namespace std::string_view_literals;
    auto* print_output{ new LineEditWithLabel{ "Output &Filename", "Filename" } };
    m_PrintOutput = print_output->GetWidget();

    auto* render_alignment_button{ new QPushButton{ "Alignment Test" } };

    m_RenderHeader = new QCheckBox{ "Render Header" };
    m_RenderHeader->setToolTip("Determines whether the header of each page will be rendered or not.");
    EnableOptionWidgetForDefaults(m_RenderHeader, config_reqs, "render_header");

    WidgetWithLabel* card_size;
    {
        m_CardSize = MakeComboBox(
            m_ViewModel.GetCardSizes() | c_CardSizeNames,
            m_ViewModel.GetCardSizes() | c_CardSizeHints,
            "Card Size");

        auto* card_size_edit{ new QToolButton };
        card_size_edit->setIcon(QIcon{ QPixmap{ ":/res/edit.png" } });
        card_size_edit->setIconSize(card_size_edit->iconSize() - QSize{ 1, 1 }); // fixes awkward sizing of icon
        card_size_edit->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonIconOnly);
        card_size_edit->setToolTip("Edit, add, or remove available card sizes.");

        QObject::connect(card_size_edit,
                         &QToolButton::pressed,
                         this,
                         &PrintOptionsWidget::OpenCardSizesPopup);

        auto* card_size_layout{ new QHBoxLayout };
        card_size_layout->addWidget(m_CardSize);
        card_size_layout->addWidget(card_size_edit);
        card_size_layout->setContentsMargins(0, 0, 0, 0);

        auto* card_size_widget{ new QWidget };
        card_size_widget->setLayout(card_size_layout);

        card_size = new WidgetWithLabel{
            "&Card Size",
            card_size_widget
        };
        card_size->setToolTip("Additional card sizes can be defined in config.ini\n\nNote: Card size will be accurate in the rendered PDF but only the quantity of cards per page is accurately displayed in the preview.");
    }
    EnableOptionWidgetForDefaults(m_CardSize, config_reqs, "card_size");

    WidgetWithLabel* paper_size;
    {
        m_PaperSize = MakeComboBox(
            m_ViewModel.GetPageSizes() | c_PageSizeNames,
            "Page Size");

        auto* paper_size_edit{ new QToolButton };
        paper_size_edit->setIcon(QIcon{ QPixmap{ ":/res/edit.png" } });
        paper_size_edit->setIconSize(paper_size_edit->iconSize() - QSize{ 1, 1 }); // fixes awkward sizing of icon
        paper_size_edit->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonIconOnly);
        paper_size_edit->setToolTip("Edit, add, or remove available card sizes.");

        QObject::connect(paper_size_edit,
                         &QToolButton::pressed,
                         this,
                         &PrintOptionsWidget::OpenPageSizesPopup);

        auto* paper_size_layout{ new QHBoxLayout };
        paper_size_layout->addWidget(m_PaperSize);
        paper_size_layout->addWidget(paper_size_edit);
        paper_size_layout->setContentsMargins(0, 0, 0, 0);

        auto* paper_size_widget{ new QWidget };
        paper_size_widget->setLayout(paper_size_layout);

        paper_size = new WidgetWithLabel{
            "&Paper Size",
            paper_size_widget
        };
        paper_size->setToolTip("Additional card sizes can be defined in config.ini");
    }
    EnableOptionWidgetForDefaults(m_PaperSize, config_reqs, "page_size");

    m_BasePdf = new ComboBoxWithLabel{
        "&Base Pdf", m_ViewModel.GetBasePdfNames(), "Base Pdf"
    };
    EnableOptionWidgetForDefaults(m_BasePdf->GetWidget(), config_reqs, "base_pdf");

    m_Orientation = new ComboBoxWithLabel{
        "&Orientation", magic_enum::enum_names<PageOrientation>(), magic_enum::enum_name(PageOrientation::Portrait)
    };
    EnableOptionWidgetForDefaults(m_Orientation->GetWidget(), config_reqs, "orientation");

    auto* paper_info{ new LabelWithLabel{ "", SizeToString({ 15_mm, 42_mm }, base_unit) } };
    m_PaperInfo = paper_info->GetWidget();

    auto* cards_info{ new LabelWithLabel{ "Cards Size", SizeToString({ 15_mm, 42_mm }, base_unit) } };
    m_CardsInfo = cards_info->GetWidget();
    m_CardsInfo->setToolTip("Size of the cards area in the final rendered PDF (excluding margins)");

    auto* left_margin{ new LengthSpinBoxWithLabel{ "&Left Margin", base_unit } };
    m_LeftMarginSpin = static_cast<LengthSpinBox*>(left_margin->GetWidget());
    m_LeftMarginSpin->ConnectUnitSignals(this);
    m_LeftMarginSpin->setDecimals(2);
    m_LeftMarginSpin->setSingleStep(0.1);

    auto* top_margin{ new LengthSpinBoxWithLabel{ "&Top Margin", base_unit } };
    m_TopMarginSpin = static_cast<LengthSpinBox*>(top_margin->GetWidget());
    m_TopMarginSpin->ConnectUnitSignals(this);
    m_TopMarginSpin->setDecimals(2);
    m_TopMarginSpin->setSingleStep(0.1);

    auto* right_margin{ new LengthSpinBoxWithLabel{ "&Right Margin", base_unit } };
    m_RightMarginSpin = static_cast<LengthSpinBox*>(right_margin->GetWidget());
    m_RightMarginSpin->ConnectUnitSignals(this);
    m_RightMarginSpin->setDecimals(2);
    m_RightMarginSpin->setSingleStep(0.1);

    auto* bottom_margin{ new LengthSpinBoxWithLabel{ "&Bottom Margin", base_unit } };
    m_BottomMarginSpin = static_cast<LengthSpinBox*>(bottom_margin->GetWidget());
    m_BottomMarginSpin->ConnectUnitSignals(this);
    m_BottomMarginSpin->setDecimals(2);
    m_BottomMarginSpin->setSingleStep(0.1);

    auto* margins_mode{ new ComboBoxWithLabel{
        "&Margin Mode",
        magic_enum::enum_names<MarginsMode>(),
        magic_enum::enum_name(MarginsMode::Auto) } };
    m_MarginsMode = margins_mode->GetWidget();
    EnableOptionWidgetForDefaults(m_MarginsMode, config_reqs, "margins_mode");

    auto* all_margins{ new LengthSpinBoxWithLabel{ "&All Margins", base_unit } };
    m_AllMarginsSpin = static_cast<LengthSpinBox*>(all_margins->GetWidget());
    m_AllMarginsSpin->ConnectUnitSignals(this);
    m_AllMarginsSpin->setDecimals(2);
    m_AllMarginsSpin->setSingleStep(0.1);

    auto* card_orientation{ new ComboBoxWithLabel{
        "Card Orien&tation",
        magic_enum::enum_names<CardOrientation>(),
        magic_enum::enum_name(CardOrientation::Vertical) } };
    m_CardOrientation = card_orientation->GetWidget();
    EnableOptionWidgetForDefaults(m_CardOrientation, config_reqs, "card_orientation");

    {
        m_CardsWidthVertical = MakeDoubleSpinBox();
        m_CardsWidthVertical->setDecimals(0);
        m_CardsWidthVertical->setRange(1, 10);
        m_CardsWidthVertical->setSingleStep(1);
        m_CardsHeightVertical = MakeDoubleSpinBox();
        m_CardsHeightVertical->setDecimals(0);
        m_CardsHeightVertical->setRange(1, 10);
        m_CardsHeightVertical->setSingleStep(1);
        auto* cards_layout_vertical_layout{ new QHBoxLayout };
        cards_layout_vertical_layout->addWidget(m_CardsWidthVertical);
        cards_layout_vertical_layout->addWidget(m_CardsHeightVertical);
        cards_layout_vertical_layout->setContentsMargins(0, 0, 0, 0);
        auto* cards_layout_vertical_container{ new QWidget };
        cards_layout_vertical_container->setLayout(cards_layout_vertical_layout);
        m_CardsLayoutVertical = new WidgetWithLabel("&Vertical Layout", cards_layout_vertical_container);

        EnableOptionWidgetForDefaults(m_CardsWidthVertical, config_reqs, "card_layout_vertical.width");
        EnableOptionWidgetForDefaults(m_CardsHeightVertical, config_reqs, "card_layout_vertical.height");
    }

    {
        m_CardsWidthHorizontal = MakeDoubleSpinBox();
        m_CardsWidthHorizontal->setDecimals(0);
        m_CardsWidthHorizontal->setRange(1, 10);
        m_CardsWidthHorizontal->setSingleStep(1);
        m_CardsHeightHorizontal = MakeDoubleSpinBox();
        m_CardsHeightHorizontal->setDecimals(0);
        m_CardsHeightHorizontal->setRange(1, 10);
        m_CardsHeightHorizontal->setSingleStep(1);
        auto* cards_layout_horizontal_layout{ new QHBoxLayout };
        cards_layout_horizontal_layout->addWidget(m_CardsWidthHorizontal);
        cards_layout_horizontal_layout->addWidget(m_CardsHeightHorizontal);
        cards_layout_horizontal_layout->setContentsMargins(0, 0, 0, 0);
        auto* cards_layout_horizontal_container{ new QWidget };
        cards_layout_horizontal_container->setLayout(cards_layout_horizontal_layout);
        m_CardsLayoutHorizontal = new WidgetWithLabel("&Horizontal Layout", cards_layout_horizontal_container);

        EnableOptionWidgetForDefaults(m_CardsWidthHorizontal, config_reqs, "card_layout_horizontal.width");
        EnableOptionWidgetForDefaults(m_CardsHeightHorizontal, config_reqs, "card_layout_horizontal.height");
    }

    auto* flip_on{ new ComboBoxWithLabel{
        "Fl&ip On", magic_enum::enum_names<FlipPageOn>(), magic_enum::enum_name(FlipPageOn::LeftEdge) } };
    m_FlipOn = flip_on->GetWidget();
    EnableOptionWidgetForDefaults(m_FlipOn, config_reqs, "flip_page_on");

    auto* layout{ new QVBoxLayout };
    layout->addWidget(render_alignment_button);
    layout->addWidget(print_output);
    layout->addWidget(m_RenderHeader);
    layout->addWidget(card_size);
    layout->addWidget(paper_size);
    layout->addWidget(m_BasePdf);
    layout->addWidget(m_Orientation);
    layout->addWidget(paper_info);
    layout->addWidget(cards_info);
    layout->addWidget(margins_mode);
    layout->addWidget(left_margin);
    layout->addWidget(top_margin);
    layout->addWidget(right_margin);
    layout->addWidget(bottom_margin);
    layout->addWidget(all_margins);
    layout->addWidget(card_orientation);
    layout->addWidget(m_CardsLayoutVertical);
    layout->addWidget(m_CardsLayoutHorizontal);
    layout->addWidget(flip_on);
    setLayout(layout);

    const auto change_cards_layout_vertical{
        [this]()
        {
            m_ViewModel.ChangeCardsLayoutVertical(
                m_CardsWidthVertical->value(),
                m_CardsHeightVertical->value());
        }
    };
    const auto change_cards_layout_horizontal{
        [this]()
        {
            m_ViewModel.ChangeCardsLayoutHorizontal(
                m_CardsWidthHorizontal->value(),
                m_CardsHeightHorizontal->value());
        }
    };

    QObject::connect(render_alignment_button,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     [this]()
                     {
                         if (!m_ViewModel.DoRenderAlignmentTest())
                         {
                             TRACY_AUTO_SCOPE();
                             auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
                             main_window->Toast(ToastType::Error,
                                                "PDF Rendering Error",
                                                "Failure while creating pdf, please check logs for details.");
                         }
                     });
    QObject::connect(m_PrintOutput,
                     &QLineEdit::textChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangeOutputFilename);
    QObject::connect(m_RenderHeader,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangePageHeaderEnabled);
    QObject::connect(m_CardSize,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangeCardSizeChoice);
    QObject::connect(m_PaperSize,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangePageSizeChoice);
    QObject::connect(m_BasePdf->GetWidget(),
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangeBasePdf);
    QObject::connect(m_MarginsMode,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangePageMarginsMode);
    QObject::connect(m_LeftMarginSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     std::bind_front(
                         &PrintOptionsViewModel::ChangePageMargin,
                         &m_ViewModel,
                         Margin::Left));
    QObject::connect(m_TopMarginSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     std::bind_front(
                         &PrintOptionsViewModel::ChangePageMargin,
                         &m_ViewModel,
                         Margin::Top));
    QObject::connect(m_RightMarginSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     std::bind_front(
                         &PrintOptionsViewModel::ChangePageMargin,
                         &m_ViewModel,
                         Margin::Right));
    QObject::connect(m_BottomMarginSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     std::bind_front(
                         &PrintOptionsViewModel::ChangePageMargin,
                         &m_ViewModel,
                         Margin::Bottom));
    QObject::connect(m_AllMarginsSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     std::bind_front(
                         &PrintOptionsViewModel::ChangePageMargin,
                         &m_ViewModel,
                         Margin::All));
    QObject::connect(card_orientation->GetWidget(),
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangeCardOrientation);
    QObject::connect(m_CardsWidthVertical,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     change_cards_layout_vertical);
    QObject::connect(m_CardsHeightVertical,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     change_cards_layout_vertical);
    QObject::connect(m_CardsWidthHorizontal,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     change_cards_layout_horizontal);
    QObject::connect(m_CardsHeightHorizontal,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     change_cards_layout_horizontal);
    QObject::connect(m_Orientation->GetWidget(),
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangePageOrientation);
    QObject::connect(flip_on->GetWidget(),
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &PrintOptionsViewModel::ChangeFlipPageOn);

    QObject::connect(this,
                     &PrintOptionsWidget::BaseUnitChanged,
                     this,
                     [this](Unit base_unit)
                     {
                         m_PaperInfo->setText(
                             ToQString(SizeToString(
                                 m_ViewModel.GetPageSize(), base_unit)));
                         m_CardsInfo->setText(
                             ToQString(SizeToString(
                                 m_ViewModel.GetCardsSize(), base_unit)));
                     });

    FORWARD_SIGNAL_FROM_VIEW_MODEL(AdvancedModeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BaseUnitChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(AvailableCardSizesChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(AvailablePageSizesChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(AvailableBasePdfsChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(OutputFilenameChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PageHeaderEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardSizeChoiceChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PageSizeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PageSizeChoiceChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BasePdfChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardsSizeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PageMarginsModeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PageMarginsChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardOrientationChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardsLayoutVerticalChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardsLayoutHorizontalChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PageOrientationChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(FlipPageOnChanged);

    m_ViewModel.EmitDefaults();
}

void PrintOptionsWidget::AdvancedModeChanged(bool advanced_mode)
{
    // Always enabled: m_PrintOutput, m_RenderHeader, m_CardSize, m_PaperSize, m_BasePdf, m_Orientation, m_SizeInfo
    m_LeftMarginSpin->parentWidget()->setVisible(advanced_mode);
    m_TopMarginSpin->parentWidget()->setVisible(advanced_mode);
    m_RightMarginSpin->parentWidget()->setVisible(advanced_mode);
    m_BottomMarginSpin->parentWidget()->setVisible(advanced_mode);
    m_MarginsMode->parentWidget()->setVisible(advanced_mode);
    m_AllMarginsSpin->parentWidget()->setVisible(advanced_mode);
    m_CardOrientation->parentWidget()->setVisible(advanced_mode);
    m_FlipOn->parentWidget()->setVisible(advanced_mode);
}
void PrintOptionsWidget::AvailableCardSizesChanged(const CardSizes& card_sizes)
{
    UpdateComboBox(
        m_CardSize,
        card_sizes | c_CardSizeNames,
        card_sizes | c_CardSizeHints,
        m_CardSize->currentText().toStdString());
}
void PrintOptionsWidget::AvailablePageSizesChanged(const PageSizes& page_sizes)
{
    UpdateComboBox(
        m_PaperSize,
        page_sizes | c_PageSizeNames,
        m_PaperSize->currentText().toStdString());
}
void PrintOptionsWidget::AvailableBasePdfsChanged(std::span<const std::string> base_pdfs)
{
    UpdateComboBox(
        m_BasePdf->GetWidget(),
        base_pdfs,
        m_BasePdf->GetWidget()->currentText().toStdString());
}

void PrintOptionsWidget::OutputFilenameChanged(const fs::path& output_filename)
{
    m_PrintOutput->blockSignals(true);
    m_PrintOutput->setText(ToQString(output_filename));
    m_PrintOutput->blockSignals(false);
}
void PrintOptionsWidget::PageHeaderEnabledChanged(bool page_header_enabled)
{
    m_RenderHeader->blockSignals(true);
    m_RenderHeader->setChecked(page_header_enabled);
    m_RenderHeader->blockSignals(false);
}
void PrintOptionsWidget::CardSizeChoiceChanged(std::string_view card_size_choice)
{
    m_CardSize->blockSignals(true);
    m_CardSize->setCurrentText(ToQString(card_size_choice));
    m_CardSize->blockSignals(false);
}
void PrintOptionsWidget::PageSizeChanged(Size page_size)
{
    m_PaperInfo->setText(ToQString(SizeToString(page_size, m_ViewModel.GetBaseUnit())));
}
void PrintOptionsWidget::PageSizeChoiceChanged(std::string_view page_size_choice)
{
    m_PaperSize->blockSignals(true);
    m_PaperSize->setCurrentText(ToQString(page_size_choice));
    m_PaperSize->blockSignals(false);

    const auto card_orientation{ m_ViewModel.GetCardOrientation() };
    const bool fit_size{ page_size_choice == g_FitSize };
    const bool infer_size{ page_size_choice == g_BasePDFSize };

    const bool layout_vertical{ card_orientation != CardOrientation::Horizontal };
    m_CardsLayoutVertical->setEnabled(fit_size && layout_vertical);
    m_CardsLayoutVertical->setVisible(fit_size && layout_vertical);

    const bool layout_horizontal{ card_orientation != CardOrientation::Vertical };
    m_CardsLayoutHorizontal->setEnabled(fit_size && layout_horizontal);
    m_CardsLayoutHorizontal->setVisible(fit_size && layout_horizontal);

    m_Orientation->setEnabled(!fit_size && !infer_size);
    m_Orientation->setVisible(!fit_size && !infer_size);

    m_BasePdf->setEnabled(infer_size);
    m_BasePdf->setVisible(infer_size);
}
void PrintOptionsWidget::BasePdfChanged(std::string_view base_pdf)
{
    auto* base_pdf_widget{ m_BasePdf->GetWidget() };
    base_pdf_widget->blockSignals(true);
    base_pdf_widget->setCurrentText(ToQString(base_pdf));
    base_pdf_widget->blockSignals(false);
}
void PrintOptionsWidget::CardsSizeChanged(Size cards_size)
{
    m_CardsInfo->setText(ToQString(SizeToString(cards_size, m_ViewModel.GetBaseUnit())));
}
void PrintOptionsWidget::PageMarginsModeChanged(MarginsMode margins_mode)
{
    m_MarginsMode->blockSignals(true);
    m_MarginsMode->setCurrentText(ToQString(magic_enum::enum_name(margins_mode)));
    m_MarginsMode->blockSignals(false);

    const bool custom_margins{ margins_mode != MarginsMode::Auto };
    const bool margins_full_control{ margins_mode == MarginsMode::Full };
    const bool margins_linked{ margins_mode == MarginsMode::Linked };

    m_LeftMarginSpin->setEnabled(custom_margins && !margins_linked);
    m_TopMarginSpin->setEnabled(custom_margins && !margins_linked);
    m_RightMarginSpin->setEnabled(custom_margins && margins_full_control);
    m_BottomMarginSpin->setEnabled(custom_margins && margins_full_control);
    m_AllMarginsSpin->setEnabled(custom_margins && margins_linked);
}
void PrintOptionsWidget::PageMarginsChanged(Margins margins)
{
    m_LeftMarginSpin->blockSignals(true);
    m_LeftMarginSpin->SetValue(margins.m_Left);
    m_LeftMarginSpin->blockSignals(false);

    m_TopMarginSpin->blockSignals(true);
    m_TopMarginSpin->SetValue(margins.m_Top);
    m_TopMarginSpin->blockSignals(false);

    m_RightMarginSpin->blockSignals(true);
    m_RightMarginSpin->SetValue(margins.m_Right);
    m_RightMarginSpin->blockSignals(false);

    m_BottomMarginSpin->blockSignals(true);
    m_BottomMarginSpin->SetValue(margins.m_Bottom);
    m_BottomMarginSpin->blockSignals(false);

    m_AllMarginsSpin->blockSignals(true);
    m_AllMarginsSpin->SetValue(margins.m_Left);
    m_AllMarginsSpin->blockSignals(false);
}
void PrintOptionsWidget::MaxPageMarginsChanged(Size max_margins)
{
    m_LeftMarginSpin->SetRange(0_mm, max_margins.x);
    m_TopMarginSpin->SetRange(0_mm, max_margins.y);
    m_RightMarginSpin->SetRange(0_mm, max_margins.x);
    m_BottomMarginSpin->SetRange(0_mm, max_margins.y);
    m_AllMarginsSpin->SetRange(0_mm, std::min(max_margins.x, max_margins.y));
}
void PrintOptionsWidget::CardOrientationChanged(CardOrientation card_orientation)
{
    m_CardOrientation->blockSignals(true);
    m_CardOrientation->setCurrentText(ToQString(magic_enum::enum_name(card_orientation)));
    m_CardOrientation->blockSignals(false);

    const auto page_size_choice{ m_ViewModel.GetPageSizeChoice() };
    const bool fit_size{ page_size_choice == g_FitSize };

    const bool layout_vertical{ card_orientation != CardOrientation::Horizontal };
    m_CardsLayoutVertical->setEnabled(fit_size && layout_vertical);
    m_CardsLayoutVertical->setVisible(fit_size && layout_vertical);

    const bool layout_horizontal{ card_orientation != CardOrientation::Vertical };
    m_CardsLayoutHorizontal->setEnabled(fit_size && layout_horizontal);
    m_CardsLayoutHorizontal->setVisible(fit_size && layout_horizontal);
}
void PrintOptionsWidget::CardsLayoutVerticalChanged(dla::uvec2 card_layout)
{
    m_CardsWidthVertical->blockSignals(true);
    m_CardsWidthVertical->setValue(card_layout.x);
    m_CardsWidthVertical->blockSignals(false);

    m_CardsHeightVertical->blockSignals(true);
    m_CardsHeightVertical->setValue(card_layout.y);
    m_CardsHeightVertical->blockSignals(false);

    if (card_layout.x == 0 || card_layout.y == 0)
    {
        m_CardsWidthVertical->setStyleSheet("QSpinBox { color: red; }");
        m_CardsHeightVertical->setStyleSheet("QSpinBox { color: red; }");
        m_CardsWidthVertical->setToolTip("No cards can fit on the page with current settings");
        m_CardsHeightVertical->setToolTip("No cards can fit on the page with current settings");
    }
    else
    {
        m_CardsWidthVertical->setStyleSheet("");
        m_CardsHeightVertical->setStyleSheet("");
        m_CardsWidthVertical->setToolTip("");
        m_CardsHeightVertical->setToolTip("");
    }
}
void PrintOptionsWidget::CardsLayoutHorizontalChanged(dla::uvec2 card_layout)
{
    m_CardsWidthHorizontal->blockSignals(true);
    m_CardsWidthHorizontal->setValue(card_layout.x);
    m_CardsWidthHorizontal->blockSignals(false);

    m_CardsHeightHorizontal->blockSignals(true);
    m_CardsHeightHorizontal->setValue(card_layout.y);
    m_CardsHeightHorizontal->blockSignals(false);

    if (card_layout.x == 0 || card_layout.y == 0)
    {
        m_CardsWidthHorizontal->setStyleSheet("QSpinBox { color: red; }");
        m_CardsHeightHorizontal->setStyleSheet("QSpinBox { color: red; }");
        m_CardsWidthHorizontal->setToolTip("No cards can fit on the page with current settings");
        m_CardsHeightHorizontal->setToolTip("No cards can fit on the page with current settings");
    }
    else
    {
        m_CardsWidthHorizontal->setStyleSheet("");
        m_CardsHeightHorizontal->setStyleSheet("");
        m_CardsWidthHorizontal->setToolTip("");
        m_CardsHeightHorizontal->setToolTip("");
    }
}
void PrintOptionsWidget::PageOrientationChanged(PageOrientation page_orientation)
{
    auto* page_orientation_widget{ m_Orientation->GetWidget() };
    page_orientation_widget->blockSignals(true);
    page_orientation_widget->setCurrentText(ToQString(magic_enum::enum_name(page_orientation)));
    page_orientation_widget->blockSignals(false);
}
void PrintOptionsWidget::FlipPageOnChanged(FlipPageOn flip_on)
{
    m_FlipOn->blockSignals(true);
    m_FlipOn->setCurrentText(ToQString(magic_enum::enum_name(flip_on)));
    m_FlipOn->blockSignals(false);
}

void PrintOptionsWidget::OpenCardSizesPopup()
{
    CardSizePopup card_size_popup{
        nullptr,
        m_ViewModel.GetCardSizes(),
        m_ViewModel.GetDefaultCardSizes(),
        m_ViewModel.GetBaseUnit(),
    };

    QObject::connect(
        &card_size_popup,
        &CardSizePopup::CardSizesChanged,
        &m_ViewModel,
        &PrintOptionsViewModel::ChangeCardSizes);

    window()->setEnabled(false);
    card_size_popup.Show();
    window()->setEnabled(true);
}
void PrintOptionsWidget::OpenPageSizesPopup()
{
    PaperSizePopup paper_size_popup{
        nullptr,
        m_ViewModel.GetPageSizes(),
        m_ViewModel.GetDefaultPageSizes(),
    };

    QObject::connect(
        &paper_size_popup,
        &PaperSizePopup::PageSizesChanged,
        &m_ViewModel,
        &PrintOptionsViewModel::ChangePageSizes);

    window()->setEnabled(false);
    paper_size_popup.Show();
    window()->setEnabled(true);
}

std::string PrintOptionsWidget::SizeToString(Size size, Unit unit)
{
    const auto base_unit{ UnitValue(unit) };
    const auto base_unit_name{ UnitName(unit) };
    return fmt::format("{:.1f} x {:.1f} {}", size.x / base_unit, size.y / base_unit, base_unit_name);
}