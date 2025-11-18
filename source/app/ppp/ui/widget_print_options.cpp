#include <ppp/ui/widget_print_options.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QToolButton>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_combo_box.hpp>
#include <ppp/ui/widget_double_spin_box.hpp>
#include <ppp/ui/widget_label.hpp>

#include <ppp/ui/popups/card_size_popup.hpp>
#include <ppp/ui/popups/paper_size_popup.hpp>

PrintOptionsWidget::PrintOptionsWidget(Project& project)
    : m_Project{ project }
{
    setObjectName("Print Options");

    const auto initial_base_unit_name{ ToQString(UnitShortName(g_Cfg.m_BaseUnit)) };

    const auto initial_page_size{ project.ComputePageSize() };
    const auto initial_cards_size{ project.ComputeCardsSize() };

    using namespace std::string_view_literals;
    auto* print_output{ new LineEditWithLabel{ "Output &Filename", project.m_Data.m_FileName.string() } };
    m_PrintOutput = print_output->GetWidget();

    WidgetWithLabel* card_size;
    {
        m_CardSize = MakeComboBox(
            std::span<const std::string>{
                g_Cfg.m_CardSizes |
                std::views::keys |
                std::ranges::to<std::vector>() },
            std::span<const std::string>{
                g_Cfg.m_CardSizes |
                std::views::values |
                std::views::transform(&Config::CardSizeInfo::m_Hint) |
                std::ranges::to<std::vector>() },
            project.m_Data.m_CardSizeChoice);

        auto* card_size_edit{ new QToolButton };
        card_size_edit->setIcon(QIcon{ QPixmap{ ":/res/edit.png" } });
        card_size_edit->setIconSize(card_size_edit->iconSize() - QSize{ 1, 1 }); // fixes awkward sizing of icon
        card_size_edit->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonIconOnly);
        card_size_edit->setToolTip("Edit, add, or remove available card sizes.");

        QObject::connect(card_size_edit,
                         &QToolButton::pressed,
                         [this]()
                         {
                             window()->setEnabled(false);
                             {
                                 CardSizePopup card_size_popup{ nullptr, g_Cfg };

                                 QObject::connect(
                                     &card_size_popup,
                                     &CardSizePopup::CardSizesChanged,
                                     [this](const std::map<std::string, Config::CardSizeInfo>& card_sizes)
                                     {
                                         if (card_sizes.empty())
                                         {
                                             LogError("User tried to remove all card sizes. Ignoring request.");
                                             return;
                                         }

                                         g_Cfg.m_CardSizes = card_sizes;
                                         if (!g_Cfg.m_CardSizes.contains(m_Project.m_Data.m_CardSizeChoice))
                                         {
                                             m_Project.m_Data.m_CardSizeChoice = g_Cfg.m_CardSizes.begin()->first;
                                             CardSizeChanged();
                                         }

                                         UpdateComboBox(
                                             m_CardSize,
                                             std::span<const std::string>{
                                                 std::views::keys(g_Cfg.m_CardSizes) |
                                                 std::ranges::to<std::vector>() },
                                             std::span<const std::string>{
                                                 g_Cfg.m_CardSizes |
                                                 std::views::values |
                                                 std::views::transform(&Config::CardSizeInfo::m_Hint) |
                                                 std::ranges::to<std::vector>() },
                                             m_Project.m_Data.m_CardSizeChoice);
                                         CardSizesChanged();
                                     });

                                 card_size_popup.Show();
                             }
                             window()->setEnabled(true);
                         });

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

    WidgetWithLabel* paper_size;
    {
        m_PaperSize = MakeComboBox(
            std::span<const std::string>{ std::views::keys(g_Cfg.m_PageSizes) |
                                          std::ranges::to<std::vector>() },
            {},
            project.m_Data.m_PageSize);

        auto* paper_size_edit{ new QToolButton };
        paper_size_edit->setIcon(QIcon{ QPixmap{ ":/res/edit.png" } });
        paper_size_edit->setIconSize(paper_size_edit->iconSize() - QSize{ 1, 1 }); // fixes awkward sizing of icon
        paper_size_edit->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonIconOnly);
        paper_size_edit->setToolTip("Edit, add, or remove available card sizes.");

        QObject::connect(paper_size_edit,
                         &QToolButton::pressed,
                         [this]()
                         {
                             window()->setEnabled(false);
                             {
                                 PaperSizePopup paper_size_popup{ nullptr, g_Cfg };

                                 QObject::connect(
                                     &paper_size_popup,
                                     &PaperSizePopup::PageSizesChanged,
                                     [this](const std::map<std::string, Config::SizeInfo>& page_sizes)
                                     {
                                         g_Cfg.m_PageSizes = page_sizes;
                                         if (!g_Cfg.m_PageSizes.contains(m_Project.m_Data.m_PageSize))
                                         {
                                             m_Project.m_Data.m_PageSize = g_Cfg.GetFirstValidPageSize();
                                             PageSizeChanged();
                                         }

                                         UpdateComboBox(
                                             m_PaperSize,
                                             std::span<const std::string>{
                                                 std::views::keys(g_Cfg.m_PageSizes) |
                                                 std::ranges::to<std::vector>() },
                                             {},
                                             m_Project.m_Data.m_PageSize);
                                         PageSizesChanged();
                                     });

                                 paper_size_popup.Show();
                             }
                             window()->setEnabled(true);
                         });

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

    m_BasePdf = new ComboBoxWithLabel{
        "&Base Pdf", GetBasePdfNames(), project.m_Data.m_BasePdf
    };

    m_Orientation = new ComboBoxWithLabel{
        "&Orientation", magic_enum::enum_names<PageOrientation>(), magic_enum::enum_name(project.m_Data.m_Orientation)
    };

    auto* paper_info{ new LabelWithLabel{ "", SizeToString(initial_page_size) } };
    m_PaperInfo = paper_info->GetWidget();

    auto* cards_info{ new LabelWithLabel{ "Cards Size", SizeToString(initial_cards_size) } };
    m_CardsInfo = cards_info->GetWidget();
    m_CardsInfo->setToolTip("Size of the cards area in the final rendered PDF (excluding margins)");

    auto* left_margin{ new WidgetWithLabel{ "&Left Margin", MakeDoubleSpinBox() } };
    m_LeftMarginSpin = static_cast<QDoubleSpinBox*>(left_margin->GetWidget());
    m_LeftMarginSpin->setDecimals(2);
    m_LeftMarginSpin->setSingleStep(0.1);
    m_LeftMarginSpin->setSuffix(initial_base_unit_name);

    auto* top_margin{ new WidgetWithLabel{ "&Top Margin", MakeDoubleSpinBox() } };
    m_TopMarginSpin = static_cast<QDoubleSpinBox*>(top_margin->GetWidget());
    m_TopMarginSpin->setDecimals(2);
    m_TopMarginSpin->setSingleStep(0.1);
    m_TopMarginSpin->setSuffix(initial_base_unit_name);

    auto* right_margin{ new WidgetWithLabel{ "&Right Margin", MakeDoubleSpinBox() } };
    m_RightMarginSpin = static_cast<QDoubleSpinBox*>(right_margin->GetWidget());
    m_RightMarginSpin->setDecimals(2);
    m_RightMarginSpin->setSingleStep(0.1);
    m_RightMarginSpin->setSuffix(initial_base_unit_name);

    auto* bottom_margin{ new WidgetWithLabel{ "&Bottom Margin", MakeDoubleSpinBox() } };
    m_BottomMarginSpin = static_cast<QDoubleSpinBox*>(bottom_margin->GetWidget());
    m_BottomMarginSpin->setDecimals(2);
    m_BottomMarginSpin->setSingleStep(0.1);
    m_BottomMarginSpin->setSuffix(initial_base_unit_name);

    auto* margins_mode{ new ComboBoxWithLabel{
        "&Margin Mode",
        magic_enum::enum_names<MarginsMode>(),
        magic_enum::enum_name(project.m_Data.m_MarginsMode) } };
    m_MarginsMode = margins_mode->GetWidget();

    auto* all_margins{ new WidgetWithLabel{ "&All Margins", MakeDoubleSpinBox() } };
    m_AllMarginsSpin = static_cast<QDoubleSpinBox*>(all_margins->GetWidget());
    m_AllMarginsSpin->setDecimals(2);
    m_AllMarginsSpin->setSingleStep(0.1);
    m_AllMarginsSpin->setSuffix(initial_base_unit_name);

    auto* card_orientation{ new ComboBoxWithLabel{
        "Card Orien&tation",
        magic_enum::enum_names<CardOrientation>(),
        magic_enum::enum_name(project.m_Data.m_CardOrientation) } };
    m_CardOrientation = card_orientation->GetWidget();

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
    }

    auto* flip_on{ new ComboBoxWithLabel{
        "Fl&ip On", magic_enum::enum_names<FlipPageOn>(), magic_enum::enum_name(project.m_Data.m_FlipOn) } };
    m_FlipOn = flip_on->GetWidget();

    auto* layout{ new QVBoxLayout };
    layout->addWidget(print_output);
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

    SetDefaults();

    auto change_output{
        [=, this](QString t)
        {
            m_Project.m_Data.m_FileName = t.toStdString();
        }
    };

    auto change_papersize{
        [=, this](QString t)
        {
            m_Project.m_Data.m_PageSize = t.toStdString();

            const bool fit_size{ m_Project.m_Data.m_PageSize == Config::c_FitSize };
            const bool infer_size{ m_Project.m_Data.m_PageSize == Config::c_BasePDFSize };
            const bool layout_vertical{ m_Project.m_Data.m_CardOrientation != CardOrientation::Horizontal };
            const bool layout_horizontal{ m_Project.m_Data.m_CardOrientation != CardOrientation::Vertical };

            m_BasePdf->setEnabled(infer_size);
            m_BasePdf->setVisible(infer_size);
            m_CardsLayoutVertical->setEnabled(fit_size && layout_vertical);
            m_CardsLayoutVertical->setVisible(fit_size && layout_vertical);
            m_CardsLayoutHorizontal->setEnabled(fit_size && layout_horizontal);
            m_CardsLayoutHorizontal->setVisible(fit_size && layout_horizontal);
            m_Orientation->setEnabled(!fit_size && !infer_size);
            m_Orientation->setVisible(!fit_size && !infer_size);

            RefreshMargins(true);

            RefreshSizes();
            PageSizeChanged();
        }
    };

    auto change_base_pdf{
        [=, this](QString t)
        {
            const bool infer_size{ m_Project.m_Data.m_PageSize == Config::c_BasePDFSize };
            if (!infer_size)
            {
                return;
            }

            m_Project.m_Data.m_BasePdf = t.toStdString();

            RefreshSizes();
            RefreshMargins(false);

            PageSizeChanged();
        }
    };

    auto change_cardsize{
        [=, this](QString t)
        {
            std::string new_choice{ t.toStdString() };
            if (new_choice == m_Project.m_Data.m_CardSizeChoice)
            {
                return;
            }

            m_Project.m_Data.m_CardSizeChoice = std::move(new_choice);
            CardSizeChanged();

            // Refresh anything needed for size change
            RefreshSizes();
            RefreshMargins(false);
        }
    };

    auto on_margins_changed{
        [this]()
        {
            // Recalculate card layout and refresh UI to reflect margin changes
            // This ensures the preview accurately represents the final output
            m_Project.CacheCardLayout();
            RefreshSizes();
            RefreshMargins(false);
            MarginsChanged();
        }
    };

    auto change_margins_mode{
        [=, this](const QString& t)
        {
            const auto margins_mode{ magic_enum::enum_cast<MarginsMode>(t.toStdString())
                                         .value_or(MarginsMode::Simple) };
            m_Project.SetMarginsMode(margins_mode);

            on_margins_changed();
        }
    };

    enum class Margin
    {
        Left,
        Top,
        Right,
        Bottom,
    };

    auto change_margin{
        [=, this](Margin margin, double v)
        {
            if (!m_Project.m_Data.m_CustomMargins.has_value())
            {
                return;
            }

            auto& custom_margins{ m_Project.m_Data.m_CustomMargins.value() };
            if (margin == Margin::Right || margin == Margin::Bottom)
            {
                if (!custom_margins.m_BottomRight.has_value())
                {
                    return;
                }
            }

            // Convert UI value to internal units and update project data
            // Real-time updates ensure immediate visual feedback in preview
            const auto base_unit{ UnitValue(g_Cfg.m_BaseUnit) };
            const auto margin_value{ static_cast<float>(v) * base_unit };
            switch (margin)
            {
            case Margin::Left:
                custom_margins.m_TopLeft.x = margin_value;
                break;
            case Margin::Top:
                custom_margins.m_TopLeft.y = margin_value;
                break;
            case Margin::Right:
                custom_margins.m_BottomRight->x = margin_value;
                break;
            case Margin::Bottom:
                custom_margins.m_BottomRight->y = margin_value;
                break;
            }

            if (m_Project.m_Data.m_MarginsMode == MarginsMode::Full)
            {
                // In full control adjust margins to make sure we fit at least one card
                const auto margins{ m_Project.ComputeMargins() };
                const auto page_size{ m_Project.ComputePageSize() };
                const auto card_size{ m_Project.CardSizeWithBleed() };
                switch (margin)
                {
                case Margin::Left:
                    if (page_size.x - margins.m_Left - margins.m_Right < card_size.x)
                    {
                        custom_margins.m_BottomRight->x =
                            page_size.x - margins.m_Left - card_size.x;
                    }
                    break;
                case Margin::Top:
                    if (page_size.y - margins.m_Top - margins.m_Bottom < card_size.y)
                    {
                        custom_margins.m_BottomRight->y =
                            page_size.y - margins.m_Top - card_size.y;
                    }
                    break;
                case Margin::Right:
                    if (page_size.x - margins.m_Left - margins.m_Right < card_size.x)
                    {
                        custom_margins.m_TopLeft.x =
                            page_size.x - margins.m_Right - card_size.x;
                    }
                    break;
                case Margin::Bottom:
                    if (page_size.y - margins.m_Top - margins.m_Bottom < card_size.y)
                    {
                        custom_margins.m_TopLeft.y =
                            page_size.y - margins.m_Bottom - card_size.y;
                    }
                    break;
                }
            }

            on_margins_changed();
        }
    };

    auto change_all_margins{
        [=, this](double v)
        {
            if (!m_Project.m_Data.m_CustomMargins.has_value())
            {
                return;
            }

            auto& custom_margins{ m_Project.m_Data.m_CustomMargins.value() };
            if (!custom_margins.m_BottomRight.has_value())
            {
                return;
            }

            // Convert UI value to internal units and update project data for all margins
            // This ensures the project data is properly synchronized with the UI
            const auto base_unit{ UnitValue(g_Cfg.m_BaseUnit) };
            const auto margin_value{ static_cast<float>(v) * base_unit };
            custom_margins.m_TopLeft.x = margin_value;
            custom_margins.m_TopLeft.y = margin_value;
            custom_margins.m_BottomRight->x = margin_value;
            custom_margins.m_BottomRight->y = margin_value;

            on_margins_changed();
        }
    };

    auto change_card_orientation{
        [=, this](QString t)
        {
            m_Project.m_Data.m_CardOrientation = magic_enum::enum_cast<CardOrientation>(t.toStdString())
                                                     .value_or(CardOrientation::Vertical);

            const bool fit_size{ m_Project.m_Data.m_PageSize == Config::c_FitSize };
            if (fit_size)
            {
                const bool layout_vertical{ m_Project.m_Data.m_CardOrientation != CardOrientation::Horizontal };
                m_CardsLayoutVertical->setEnabled(layout_vertical);
                m_CardsLayoutVertical->setVisible(layout_vertical);

                const bool layout_horizontal{ m_Project.m_Data.m_CardOrientation != CardOrientation::Vertical };
                m_CardsLayoutHorizontal->setEnabled(layout_horizontal);
                m_CardsLayoutHorizontal->setVisible(layout_horizontal);
            }

            if (m_Project.CacheCardLayout())
            {
                CardLayoutChanged();
            }
            CardOrientationChanged();

            RefreshSizes();
            RefreshMargins(false);
        }
    };
    auto change_cards_width_vertical{
        [=, this](double v)
        {
            m_Project.m_Data.m_CardLayoutVertical.x = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_cards_height_vertical{
        [=, this](double v)
        {
            m_Project.m_Data.m_CardLayoutVertical.y = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_cards_width_horizontal{
        [=, this](double v)
        {
            m_Project.m_Data.m_CardLayoutHorizontal.x = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_cards_height_horizontal{
        [=, this](double v)
        {
            m_Project.m_Data.m_CardLayoutHorizontal.y = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_orientation{
        [=, this](QString t)
        {
            m_Project.m_Data.m_Orientation = magic_enum::enum_cast<PageOrientation>(t.toStdString())
                                                 .value_or(PageOrientation::Portrait);

            const bool fit_size{ m_Project.m_Data.m_PageSize == Config::c_FitSize };
            const bool infer_size{ m_Project.m_Data.m_PageSize == Config::c_BasePDFSize };
            if (fit_size || infer_size)
            {
                return;
            }

            // Clear custom margins when orientation changes
            m_Project.m_Data.m_CustomMargins.reset();

            RefreshSizes();
            RefreshMargins(true);

            OrientationChanged();
        }
    };

    auto change_flip_on{
        [=, this](QString t)
        {
            m_Project.m_Data.m_FlipOn = magic_enum::enum_cast<FlipPageOn>(t.toStdString())
                                            .value_or(FlipPageOn::LeftEdge);
            FlipOnChanged();
        }
    };

    QObject::connect(print_output->GetWidget(),
                     &QLineEdit::textChanged,
                     this,
                     change_output);
    QObject::connect(m_CardSize,
                     &QComboBox::currentTextChanged,
                     this,
                     change_cardsize);
    QObject::connect(m_PaperSize,
                     &QComboBox::currentTextChanged,
                     this,
                     change_papersize);
    QObject::connect(m_BasePdf->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_base_pdf);
    QObject::connect(m_MarginsMode,
                     &QComboBox::currentTextChanged,
                     this,
                     change_margins_mode);
    QObject::connect(m_LeftMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     std::bind_front(change_margin, Margin::Left));
    QObject::connect(m_TopMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     std::bind_front(change_margin, Margin::Top));
    QObject::connect(m_RightMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     std::bind_front(change_margin, Margin::Right));
    QObject::connect(m_BottomMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     std::bind_front(change_margin, Margin::Bottom));
    QObject::connect(m_AllMarginsSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_all_margins);
    QObject::connect(card_orientation->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_card_orientation);
    QObject::connect(m_CardsWidthVertical,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_cards_width_vertical);
    QObject::connect(m_CardsHeightVertical,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_cards_height_vertical);
    QObject::connect(m_CardsWidthHorizontal,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_cards_width_horizontal);
    QObject::connect(m_CardsHeightHorizontal,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_cards_height_horizontal);
    QObject::connect(m_Orientation->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_orientation);
    QObject::connect(flip_on->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_flip_on);
}

void PrintOptionsWidget::NewProjectOpened()
{
    SetDefaults();
}

void PrintOptionsWidget::BleedChanged()
{
    RefreshSizes();
    RefreshMargins(false);
}

void PrintOptionsWidget::SpacingChanged()
{
    RefreshSizes();
    RefreshMargins(false);
}

void PrintOptionsWidget::AdvancedModeChanged()
{
    SetAdvancedWidgetsVisibility();
}

void PrintOptionsWidget::BaseUnitChanged()
{
    const auto base_unit{ UnitValue(g_Cfg.m_BaseUnit) };
    const auto base_unit_name{ ToQString(UnitShortName(g_Cfg.m_BaseUnit)) };

    const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
    const auto margins{ m_Project.ComputeMargins() / base_unit };

    m_LeftMarginSpin->blockSignals(true);
    m_LeftMarginSpin->setSuffix(base_unit_name);
    m_LeftMarginSpin->setRange(0, max_margins.x);
    m_LeftMarginSpin->setValue(margins.m_Left);
    m_LeftMarginSpin->blockSignals(true);
    m_TopMarginSpin->blockSignals(false);
    m_TopMarginSpin->setSuffix(base_unit_name);
    m_TopMarginSpin->setRange(0, max_margins.y);
    m_TopMarginSpin->setValue(margins.m_Top);
    m_TopMarginSpin->blockSignals(true);
    m_RightMarginSpin->blockSignals(false);
    m_RightMarginSpin->setSuffix(base_unit_name);
    m_RightMarginSpin->setRange(0, max_margins.x);
    m_RightMarginSpin->setValue(margins.m_Right);
    m_RightMarginSpin->blockSignals(true);
    m_BottomMarginSpin->blockSignals(false);
    m_BottomMarginSpin->setSuffix(base_unit_name);
    m_BottomMarginSpin->setRange(0, max_margins.y);
    m_BottomMarginSpin->setValue(margins.m_Top);
    m_BottomMarginSpin->blockSignals(false);

    m_AllMarginsSpin->blockSignals(true);
    m_AllMarginsSpin->setSuffix(base_unit_name);
    m_AllMarginsSpin->setRange(0, std::max(max_margins.x, max_margins.y));
    if (m_Project.m_Data.m_MarginsMode == MarginsMode::Linked)
    {
        m_AllMarginsSpin->setValue(margins.m_Top);
    }
    m_AllMarginsSpin->blockSignals(false);
}

void PrintOptionsWidget::RenderBackendChanged()
{
    const int base_pdf_size_idx{
        [&]()
        {
            for (int i = 0; i < m_PaperSize->count(); i++)
            {
                if (m_PaperSize->itemText(i).toStdString() == Config::c_BasePDFSize)
                {
                    return i;
                }
            }
            return -1;
        }()
    };
    const bool has_base_pdf_option{ base_pdf_size_idx >= 0 };
    const bool has_base_pdf_confg{ g_Cfg.m_PageSizes.contains(std::string{ Config::c_BasePDFSize }) };
    const bool need_add_base_pdf_option{ !has_base_pdf_option && has_base_pdf_confg };
    const bool need_remove_base_pdf_option{ has_base_pdf_option && !has_base_pdf_confg };

    if (need_add_base_pdf_option || need_remove_base_pdf_option)
    {
        if (need_add_base_pdf_option)
        {
            m_PaperSize->addItem(ToQString(Config::c_BasePDFSize));
        }
        else
        {
            m_PaperSize->removeItem(base_pdf_size_idx);
        }
        m_PaperSize->setCurrentText(ToQString(m_Project.m_Data.m_PageSize));
    }

    if (g_Cfg.m_Backend != PdfBackend::PoDoFo && m_Project.m_Data.m_PageSize == Config::c_BasePDFSize)
    {
        m_PaperSize->setCurrentText(ToQString(g_Cfg.m_DefaultPageSize));
        m_Project.m_Data.m_PageSize = g_Cfg.m_DefaultPageSize;
        PageSizeChanged();
    }
}

void PrintOptionsWidget::ExternalCardSizeChanged()
{
    m_CardSize->setCurrentText(ToQString(m_Project.m_Data.m_CardSizeChoice));
    RefreshSizes();
    RefreshMargins(false);
    CardSizeChanged();
}

void PrintOptionsWidget::SetDefaults()
{
    m_PrintOutput->setText(ToQString(m_Project.m_Data.m_FileName));
    m_CardSize->setCurrentText(ToQString(m_Project.m_Data.m_CardSizeChoice));
    m_PaperSize->setCurrentText(ToQString(m_Project.m_Data.m_PageSize));
    m_CardOrientation->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_CardOrientation)));

    const bool fit_size{ m_Project.m_Data.m_PageSize == Config::c_FitSize };
    const bool infer_size{ m_Project.m_Data.m_PageSize == Config::c_BasePDFSize };

    const bool layout_vertical{ m_Project.m_Data.m_CardOrientation != CardOrientation::Horizontal };
    m_CardsLayoutVertical->setEnabled(fit_size && layout_vertical);
    m_CardsLayoutVertical->setVisible(fit_size && layout_vertical);

    const bool layout_horizontal{ m_Project.m_Data.m_CardOrientation != CardOrientation::Vertical };
    m_CardsLayoutHorizontal->setEnabled(fit_size && layout_horizontal);
    m_CardsLayoutHorizontal->setVisible(fit_size && layout_horizontal);

    m_CardsWidthVertical->setValue(m_Project.m_Data.m_CardLayoutVertical.x);
    m_CardsHeightVertical->setValue(m_Project.m_Data.m_CardLayoutVertical.y);
    m_CardsWidthHorizontal->setValue(m_Project.m_Data.m_CardLayoutHorizontal.x);
    m_CardsHeightHorizontal->setValue(m_Project.m_Data.m_CardLayoutHorizontal.y);

    // Update card layout display and warnings
    RefreshCardLayout();

    m_Orientation->setEnabled(!fit_size && !infer_size);
    m_Orientation->setVisible(!fit_size && !infer_size);
    m_Orientation->GetWidget()->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_Orientation)));

    m_FlipOn->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_FlipOn)));

    m_BasePdf->GetWidget()->setCurrentText(ToQString(m_Project.m_Data.m_BasePdf));
    m_BasePdf->setEnabled(infer_size);
    m_BasePdf->setVisible(infer_size);

    const auto base_unit{ UnitValue(g_Cfg.m_BaseUnit) };
    const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
    const auto margins{ m_Project.ComputeMargins() / base_unit };

    const auto margins_mode{ m_Project.m_Data.m_MarginsMode };
    const bool custom_margins{ margins_mode != MarginsMode::Auto };
    const bool margins_full_control{ margins_mode == MarginsMode::Full };
    const bool margins_linked{ margins_mode == MarginsMode::Linked };

    m_LeftMarginSpin->setRange(0, max_margins.x);
    m_LeftMarginSpin->setValue(margins.m_Left);
    m_LeftMarginSpin->setEnabled(custom_margins && !margins_linked);

    m_TopMarginSpin->setRange(0, max_margins.y);
    m_TopMarginSpin->setValue(margins.m_Top);
    m_TopMarginSpin->setEnabled(custom_margins && !margins_linked);

    m_RightMarginSpin->setRange(0, max_margins.x);
    m_RightMarginSpin->setValue(margins.m_Right);
    m_RightMarginSpin->setEnabled(custom_margins && margins_full_control);

    m_BottomMarginSpin->setRange(0, max_margins.y);
    m_BottomMarginSpin->setValue(margins.m_Bottom);
    m_BottomMarginSpin->setEnabled(custom_margins && margins_full_control);

    // Set up All Margins control
    m_AllMarginsSpin->setRange(0, std::min(max_margins.x, max_margins.y));
    m_AllMarginsSpin->setValue(m_LeftMarginSpin->value());
    m_AllMarginsSpin->setEnabled(custom_margins && margins_linked);

    // Set up margin mode toggle
    m_MarginsMode->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_MarginsMode)));

    SetAdvancedWidgetsVisibility();
}

void PrintOptionsWidget::SetAdvancedWidgetsVisibility()
{
    // Always enabled: m_PrintOutput, m_CardSize, m_PaperSize, m_BasePdf, m_Orientation, m_SizeInfo
    m_LeftMarginSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_TopMarginSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_RightMarginSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_BottomMarginSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_MarginsMode->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_AllMarginsSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_CardOrientation->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_FlipOn->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
}

void PrintOptionsWidget::RefreshSizes()
{
    RefreshCardLayout();

    m_PaperInfo->setText(ToQString(SizeToString(m_Project.ComputePageSize())));
    m_CardsInfo->setText(ToQString(SizeToString(m_Project.ComputeCardsSize())));
}

void PrintOptionsWidget::RefreshMargins(bool reset_margins)
{
    const auto base_unit{ UnitValue(g_Cfg.m_BaseUnit) };
    const auto margins_mode{ m_Project.m_Data.m_MarginsMode };
    const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };

    m_LeftMarginSpin->setRange(0, max_margins.x);
    m_TopMarginSpin->setRange(0, max_margins.y);
    m_RightMarginSpin->setRange(0, max_margins.x);
    m_BottomMarginSpin->setRange(0, max_margins.y);
    m_AllMarginsSpin->setRange(0, std::min(max_margins.x, max_margins.y));

    if (reset_margins)
    {
        if (margins_mode == MarginsMode::Linked)
        {
            const auto default_margins{ m_Project.ComputeDefaultMargins() / base_unit };
            const auto default_all_margins{ std::min(default_margins.x, default_margins.y) };
            m_LeftMarginSpin->setValue(default_all_margins);
            m_TopMarginSpin->setValue(default_all_margins);
            m_RightMarginSpin->setValue(default_all_margins);
            m_BottomMarginSpin->setValue(default_all_margins);
            m_AllMarginsSpin->setValue(default_all_margins);
        }
        else
        {
            const auto default_margins{ m_Project.ComputeDefaultMargins() / base_unit };
            m_LeftMarginSpin->setValue(default_margins.x);
            m_TopMarginSpin->setValue(default_margins.y);
            m_RightMarginSpin->setValue(default_margins.x);
            m_BottomMarginSpin->setValue(default_margins.y);
            m_AllMarginsSpin->setValue(0.0);
        }
    }
    else
    {
        // Temporarily block signals to prevent recursive calls during refresh
        m_LeftMarginSpin->blockSignals(true);
        m_TopMarginSpin->blockSignals(true);
        m_RightMarginSpin->blockSignals(true);
        m_BottomMarginSpin->blockSignals(true);
        m_AllMarginsSpin->blockSignals(true);

        // Preserve current margin values after updating ranges while also pulling
        // exact computed margins in case they are dependent on each other
        const auto margins{ m_Project.ComputeMargins() / base_unit };
        m_LeftMarginSpin->setValue(margins.m_Left);
        m_TopMarginSpin->setValue(margins.m_Top);
        m_RightMarginSpin->setValue(margins.m_Right);
        m_BottomMarginSpin->setValue(margins.m_Bottom);

        if (margins_mode == MarginsMode::Linked)
        {
            m_AllMarginsSpin->setValue(margins.m_Left);
        }
        else
        {
            m_AllMarginsSpin->setValue(0.0);
        }

        // Unblock signals
        m_LeftMarginSpin->blockSignals(false);
        m_TopMarginSpin->blockSignals(false);
        m_RightMarginSpin->blockSignals(false);
        m_BottomMarginSpin->blockSignals(false);
        m_AllMarginsSpin->blockSignals(false);
    }

    // Set enabled states based on margin mode
    const bool custom_margins{ margins_mode != MarginsMode::Auto };
    const bool margins_full_control{ margins_mode == MarginsMode::Full };
    const bool margins_linked{ margins_mode == MarginsMode::Linked };
    m_LeftMarginSpin->setEnabled(custom_margins && !margins_linked);
    m_TopMarginSpin->setEnabled(custom_margins && !margins_linked);
    m_RightMarginSpin->setEnabled(custom_margins && margins_full_control);
    m_BottomMarginSpin->setEnabled(custom_margins && margins_full_control);
    m_AllMarginsSpin->setEnabled(custom_margins && margins_linked);
}

void PrintOptionsWidget::RefreshCardLayout()
{
    if (m_Project.CacheCardLayout())
    {
        m_CardsWidthVertical->setValue(m_Project.m_Data.m_CardLayoutVertical.x);
        m_CardsHeightVertical->setValue(m_Project.m_Data.m_CardLayoutVertical.y);

        m_CardsWidthHorizontal->setValue(m_Project.m_Data.m_CardLayoutHorizontal.x);
        m_CardsHeightHorizontal->setValue(m_Project.m_Data.m_CardLayoutHorizontal.y);

        // Show visual warning when no cards can fit on the page
        const auto check_valid_layout{
            [](QDoubleSpinBox* width, QDoubleSpinBox* height, const auto& layout)
            {
                if (layout.x == 0 || layout.y == 0)
                {
                    width->setStyleSheet("QSpinBox { color: red; }");
                    height->setStyleSheet("QSpinBox { color: red; }");
                    width->setToolTip("No cards can fit on the page with current settings");
                    height->setToolTip("No cards can fit on the page with current settings");
                }
                else
                {
                    width->setStyleSheet("");
                    height->setStyleSheet("");
                    width->setToolTip("");
                    height->setToolTip("");
                }
            }
        };
        check_valid_layout(m_CardsWidthVertical, m_CardsHeightVertical, m_Project.m_Data.m_CardLayoutVertical);
        check_valid_layout(m_CardsWidthHorizontal, m_CardsHeightHorizontal, m_Project.m_Data.m_CardLayoutHorizontal);
    }
}

std::vector<std::string> PrintOptionsWidget::GetBasePdfNames()
{
    std::vector<std::string> base_pdf_names{ "Empty A4" };

    QDirIterator it("./res/base_pdfs");
    while (it.hasNext())
    {
        const QFileInfo next{ it.nextFileInfo() };
        if (!next.isFile() || next.suffix().toLower() != "pdf")
        {
            continue;
        }

        std::string base_name{ next.baseName().toStdString() };
        if (std::ranges::contains(base_pdf_names, base_name))
        {
            continue;
        }

        base_pdf_names.push_back(std::move(base_name));
    }

    return base_pdf_names;
}

std::string PrintOptionsWidget::SizeToString(Size size)
{
    const auto base_unit{ UnitValue(g_Cfg.m_BaseUnit) };
    const auto base_unit_name{ UnitName(g_Cfg.m_BaseUnit) };
    return fmt::format("{:.1f} x {:.1f} {}", size.x / base_unit, size.y / base_unit, base_unit_name);
}
