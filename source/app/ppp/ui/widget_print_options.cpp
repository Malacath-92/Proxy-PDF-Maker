#include <ppp/ui/widget_print_options.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDirIterator>
#include <QHBoxLayout>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_label.hpp>

PrintOptionsWidget::PrintOptionsWidget(Project& project)
    : m_Project{ project }
{
    setObjectName("Print Options");

    const auto initial_base_unit_name{ ToQString(g_Cfg.m_BaseUnit.m_ShortName) };

    const bool initial_fit_size{ project.m_Data.m_PageSize == Config::c_FitSize };
    const bool initial_infer_size{ project.m_Data.m_PageSize == Config::c_BasePDFSize };

    const auto initial_page_size{ project.ComputePageSize() };
    const auto initial_cards_size{ project.ComputeCardsSize() };

    using namespace std::string_view_literals;
    auto* print_output{ new LineEditWithLabel{ "Output &Filename", project.m_Data.m_FileName.string() } };
    m_PrintOutput = print_output->GetWidget();

    auto* card_size{
        new ComboBoxWithLabel{
            "&Card Size",
            g_Cfg.m_CardSizes | std::views::keys | std::ranges::to<std::vector>(),
            g_Cfg.m_CardSizes |
                std::views::values |
                std::views::transform(&Config::CardSizeInfo::m_Hint) |
                std::ranges::to<std::vector>(),
            project.m_Data.m_CardSizeChoice,
        },
    };
    card_size->setToolTip("Additional card sizes can be defined in config.ini\n\nNote: Card size will be accurate in the rendered PDF but only the quantity of cards per page is accurately displayed in the preview.");
    m_CardSize = card_size->GetWidget();

    auto* paper_size{ new ComboBoxWithLabel{
        "&Paper Size", std::views::keys(g_Cfg.m_PageSizes) | std::ranges::to<std::vector>(), project.m_Data.m_PageSize } };
    paper_size->setToolTip("Additional card sizes can be defined in config.ini");
    m_PaperSize = paper_size->GetWidget();

    m_BasePdf = new ComboBoxWithLabel{
        "&Base Pdf", GetBasePdfNames(), project.m_Data.m_BasePdf
    };
    m_BasePdf->setEnabled(initial_infer_size);
    m_BasePdf->setVisible(initial_infer_size);

    auto* paper_info{ new LabelWithLabel{ "", SizeToString(initial_page_size) } };
    m_PaperInfo = paper_info->GetWidget();

    auto* cards_info{ new LabelWithLabel{ "Cards Size", SizeToString(initial_cards_size) } };
    m_CardsInfo = cards_info->GetWidget();
    m_CardsInfo->setToolTip("Size of the cards area in the final rendered PDF (excluding margins)");

    m_CustomMargins = new QCheckBox{ "&Custom Margins" };

    auto* left_margin{ new DoubleSpinBoxWithLabel{ "&Left Margin" } };
    m_LeftMarginSpin = left_margin->GetWidget();
    m_LeftMarginSpin->setDecimals(2);
    m_LeftMarginSpin->setSingleStep(0.1);
    m_LeftMarginSpin->setSuffix(initial_base_unit_name);

    auto* top_margin{ new DoubleSpinBoxWithLabel{ "&Top Margin" } };
    m_TopMarginSpin = top_margin->GetWidget();
    m_TopMarginSpin->setDecimals(2);
    m_TopMarginSpin->setSingleStep(0.1);
    m_TopMarginSpin->setSuffix(initial_base_unit_name);

    auto* right_margin{ new DoubleSpinBoxWithLabel{ "&Right Margin" } };
    m_RightMarginSpin = right_margin->GetWidget();
    m_RightMarginSpin->setDecimals(2);
    m_RightMarginSpin->setSingleStep(0.1);
    m_RightMarginSpin->setSuffix(initial_base_unit_name);

    auto* bottom_margin{ new DoubleSpinBoxWithLabel{ "&Bottom Margin" } };
    m_BottomMarginSpin = bottom_margin->GetWidget();
    m_BottomMarginSpin->setDecimals(2);
    m_BottomMarginSpin->setSingleStep(0.1);
    m_BottomMarginSpin->setSuffix(initial_base_unit_name);

    m_MarginModeToggle = new QCheckBox{ "&All Margins Mode" };

    auto* all_margins{ new DoubleSpinBoxWithLabel{ "&All Margins" } };
    m_AllMarginsSpin = all_margins->GetWidget();
    m_AllMarginsSpin->setDecimals(2);
    m_AllMarginsSpin->setSingleStep(0.1);
    m_AllMarginsSpin->setSuffix(initial_base_unit_name);

    m_CardsWidth = new QDoubleSpinBox;
    m_CardsWidth->setDecimals(0);
    m_CardsWidth->setRange(1, 10);
    m_CardsWidth->setSingleStep(1);
    m_CardsHeight = new QDoubleSpinBox;
    m_CardsHeight->setDecimals(0);
    m_CardsHeight->setRange(1, 10);
    m_CardsHeight->setSingleStep(1);
    auto* cards_layout_layout{ new QHBoxLayout };
    cards_layout_layout->addWidget(m_CardsWidth);
    cards_layout_layout->addWidget(m_CardsHeight);
    cards_layout_layout->setContentsMargins(0, 0, 0, 0);
    auto* cards_layout_container{ new QWidget };
    cards_layout_container->setLayout(cards_layout_layout);
    auto* cards_layout{ new WidgetWithLabel("Layout", cards_layout_container) };
    cards_layout->setEnabled(initial_fit_size);
    cards_layout->setVisible(initial_fit_size);

    auto* orientation{ new ComboBoxWithLabel{
        "&Orientation", magic_enum::enum_names<PageOrientation>(), magic_enum::enum_name(project.m_Data.m_Orientation) } };
    orientation->setEnabled(!initial_fit_size && !initial_infer_size);
    orientation->setVisible(!initial_fit_size && !initial_infer_size);
    m_Orientation = orientation->GetWidget();

    auto* flip_on{ new ComboBoxWithLabel{
        "Fl&ip On", magic_enum::enum_names<FlipPageOn>(), magic_enum::enum_name(project.m_Data.m_FlipOn) } };
    m_FlipOn = flip_on->GetWidget();

    SetDefaults();

    auto* layout{ new QVBoxLayout };
    layout->addWidget(print_output);
    layout->addWidget(card_size);
    layout->addWidget(paper_size);
    layout->addWidget(m_BasePdf);
    layout->addWidget(paper_info);
    layout->addWidget(cards_info);
    layout->addWidget(m_CustomMargins);
    layout->addWidget(m_MarginModeToggle);
    layout->addWidget(left_margin);
    layout->addWidget(top_margin);
    layout->addWidget(right_margin);
    layout->addWidget(bottom_margin);
    layout->addWidget(all_margins);
    layout->addWidget(cards_layout);
    layout->addWidget(orientation);
    layout->addWidget(flip_on);
    setLayout(layout);

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

            // Clear custom margins when paper size changes
            m_Project.m_Data.m_CustomMarginsFour.reset();
            m_Project.m_Data.m_CustomMargins.reset();

            RefreshSizes();

            const bool fit_size{ m_Project.m_Data.m_PageSize == Config::c_FitSize };
            const bool infer_size{ m_Project.m_Data.m_PageSize == Config::c_BasePDFSize };

            m_BasePdf->setEnabled(infer_size);
            m_BasePdf->setVisible(infer_size);
            cards_layout->setEnabled(fit_size);
            cards_layout->setVisible(fit_size);
            orientation->setEnabled(!fit_size && !infer_size);
            orientation->setVisible(!fit_size && !infer_size);

            // Reset margins to default for the new paper size
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
            const auto default_margins{ max_margins / 2.0f };

            m_LeftMarginSpin->setRange(0, max_margins.x);
            m_LeftMarginSpin->setValue(default_margins.x);
            m_TopMarginSpin->setRange(0, max_margins.y);
            m_TopMarginSpin->setValue(default_margins.y);
            m_RightMarginSpin->setRange(0, max_margins.x);
            m_RightMarginSpin->setValue(default_margins.x);
            m_BottomMarginSpin->setRange(0, max_margins.y);
            m_BottomMarginSpin->setValue(default_margins.y);

            // Update All Margins range
            m_AllMarginsSpin->setRange(0, std::max(max_margins.x, max_margins.y));

            // Disable custom margins
            m_CustomMargins->setChecked(false);
            m_LeftMarginSpin->setEnabled(false);
            m_TopMarginSpin->setEnabled(false);
            m_RightMarginSpin->setEnabled(false);
            m_BottomMarginSpin->setEnabled(false);
            m_AllMarginsSpin->setEnabled(false);
            m_MarginModeToggle->setEnabled(false);

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
            change_papersize(ToQString(m_Project.m_Data.m_PageSize));
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

            PageSizeChanged();
        }
    };

    auto switch_custom_margins{
        [this](Qt::CheckState s)
        {
            const bool custom_margins{ s == Qt::CheckState::Checked };

            if (custom_margins)
            {
                // Initialize with computed four-margin defaults to provide a reasonable starting point
                // This ensures users don't start with zero margins which could cause layout issues
                const auto default_margins_four{ m_Project.ComputeMarginsFour() };
                m_Project.m_Data.m_CustomMarginsFour.emplace(default_margins_four);
                m_Project.m_Data.m_CustomMargins.reset();
            }
            else
            {
                // Clear custom margins to return to automatic layout calculation
                // This allows the system to compute optimal margins based on page and card sizes
                m_Project.m_Data.m_CustomMarginsFour.reset();
                m_Project.m_Data.m_CustomMargins.reset();

                // Reset UI to show computed default values for immediate feedback
                const auto default_margins{ m_Project.ComputeMargins() };
                const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
                m_LeftMarginSpin->setValue(default_margins.x / base_unit);
                m_TopMarginSpin->setValue(default_margins.y / base_unit);
                m_RightMarginSpin->setValue(default_margins.x / base_unit);
                m_BottomMarginSpin->setValue(default_margins.y / base_unit);
            }

            // Enable/disable margin controls based on custom margin mode
            // This prevents confusion by only showing relevant controls
            const bool enabled{ custom_margins };
            m_LeftMarginSpin->setEnabled(enabled);
            m_TopMarginSpin->setEnabled(enabled);
            m_RightMarginSpin->setEnabled(enabled);
            m_BottomMarginSpin->setEnabled(enabled);
            m_AllMarginsSpin->setEnabled(enabled);
            m_MarginModeToggle->setEnabled(enabled);
        }
    };

    auto change_left_margin{
        [=, this](double v)
        {
            if (!m_Project.m_Data.m_CustomMarginsFour.has_value())
            {
                return;
            }

            // Convert UI value to internal units and update project data
            // Real-time updates ensure immediate visual feedback in preview
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            m_Project.m_Data.m_CustomMarginsFour.value().left = static_cast<float>(v) * base_unit;

            // Recalculate card layout and refresh UI to reflect margin changes
            // This ensures the preview accurately represents the final output
            m_Project.CacheCardLayout();
            RefreshSizes();
            MarginsChanged();
        }
    };

    auto change_top_margin{
        [=, this](double v)
        {
            if (!m_Project.m_Data.m_CustomMarginsFour.has_value())
            {
                return;
            }

            // Convert UI value to internal units and update project data
            // Real-time updates ensure immediate visual feedback in preview
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            m_Project.m_Data.m_CustomMarginsFour.value().top = static_cast<float>(v) * base_unit;

            // Recalculate card layout and refresh UI to reflect margin changes
            // This ensures the preview accurately represents the final output
            m_Project.CacheCardLayout();
            RefreshSizes();
            MarginsChanged();
        }
    };

    auto change_right_margin{
        [=, this](double v)
        {
            if (!m_Project.m_Data.m_CustomMarginsFour.has_value())
            {
                return;
            }

            // Convert UI value to internal units and update project data
            // Real-time updates ensure immediate visual feedback in preview
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            m_Project.m_Data.m_CustomMarginsFour.value().right = static_cast<float>(v) * base_unit;

            // Recalculate card layout and refresh UI to reflect margin changes
            // This ensures the preview accurately represents the final output
            m_Project.CacheCardLayout();
            RefreshSizes();
            MarginsChanged();
        }
    };

    auto change_bottom_margin{
        [=, this](double v)
        {
            if (!m_Project.m_Data.m_CustomMarginsFour.has_value())
            {
                return;
            }

            // Convert UI value to internal units and update project data
            // Real-time updates ensure immediate visual feedback in preview
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            m_Project.m_Data.m_CustomMarginsFour.value().bottom = static_cast<float>(v) * base_unit;

            // Recalculate card layout and refresh UI to reflect margin changes
            // This ensures the preview accurately represents the final output
            m_Project.CacheCardLayout();
            RefreshSizes();
            MarginsChanged();
        }
    };

    auto change_all_margins{
        [=, this](double v)
        {
            if (!m_Project.m_Data.m_CustomMarginsFour.has_value())
            {
                return;
            }

            // Synchronize all individual margin controls when using uniform margin mode
            // This ensures consistency and provides immediate visual feedback
            m_LeftMarginSpin->setValue(v);
            m_TopMarginSpin->setValue(v);
            m_RightMarginSpin->setValue(v);
            m_BottomMarginSpin->setValue(v);
        }
    };

    auto toggle_margin_mode{
        [=, this](Qt::CheckState s)
        {
            const bool all_margins_mode{ s == Qt::CheckState::Checked };

            // Keep all controls visible but enable/disable based on mode
            // This maintains UI consistency and prevents layout shifts
            m_LeftMarginSpin->setVisible(true);
            m_TopMarginSpin->setVisible(true);
            m_RightMarginSpin->setVisible(true);
            m_BottomMarginSpin->setVisible(true);
            m_AllMarginsSpin->setVisible(true);

            // Enable individual controls only when not in uniform mode
            // This prevents conflicting inputs and ensures clear user intent
            m_LeftMarginSpin->setEnabled(!all_margins_mode);
            m_TopMarginSpin->setEnabled(!all_margins_mode);
            m_RightMarginSpin->setEnabled(!all_margins_mode);
            m_BottomMarginSpin->setEnabled(!all_margins_mode);
            m_AllMarginsSpin->setEnabled(all_margins_mode);

            // Initialize uniform margin value with current individual margin
            // This provides a sensible starting point for uniform adjustments
            if (all_margins_mode)
            {
                m_AllMarginsSpin->setValue(m_LeftMarginSpin->value());
            }
        }
    };

    auto change_cards_width{
        [=, this](double v)
        {
            m_Project.m_Data.m_CardLayout.x = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_cards_height{
        [=, this](double v)
        {
            m_Project.m_Data.m_CardLayout.y = static_cast<uint32_t>(v);
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
            m_Project.m_Data.m_CustomMarginsFour.reset();
            m_Project.m_Data.m_CustomMargins.reset();

            RefreshSizes();

            // Reset margins to default for the new orientation
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
            const auto default_margins{ max_margins / 2.0f };

            m_LeftMarginSpin->setRange(0, max_margins.x);
            m_LeftMarginSpin->setValue(default_margins.x);
            m_TopMarginSpin->setRange(0, max_margins.y);
            m_TopMarginSpin->setValue(default_margins.y);
            m_RightMarginSpin->setRange(0, max_margins.x);
            m_RightMarginSpin->setValue(default_margins.x);
            m_BottomMarginSpin->setRange(0, max_margins.y);
            m_BottomMarginSpin->setValue(default_margins.y);

            // Update All Margins range
            m_AllMarginsSpin->setRange(0, std::max(max_margins.x, max_margins.y));

            // Disable custom margins
            m_CustomMargins->setChecked(false);
            m_LeftMarginSpin->setEnabled(false);
            m_TopMarginSpin->setEnabled(false);
            m_RightMarginSpin->setEnabled(false);
            m_BottomMarginSpin->setEnabled(false);
            m_AllMarginsSpin->setEnabled(false);
            m_MarginModeToggle->setEnabled(false);

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
    QObject::connect(card_size->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_cardsize);
    QObject::connect(paper_size->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_papersize);
    QObject::connect(m_BasePdf->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_base_pdf);
    QObject::connect(m_CustomMargins,
                     &QCheckBox::checkStateChanged,
                     this,
                     switch_custom_margins);
    QObject::connect(m_LeftMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_left_margin);
    QObject::connect(m_TopMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_top_margin);
    QObject::connect(m_RightMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_right_margin);
    QObject::connect(m_BottomMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_bottom_margin);
    QObject::connect(m_AllMarginsSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_all_margins);
    QObject::connect(m_MarginModeToggle,
                     &QCheckBox::checkStateChanged,
                     this,
                     toggle_margin_mode);
    QObject::connect(m_CardsWidth,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_cards_width);
    QObject::connect(m_CardsHeight,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_cards_height);
    QObject::connect(orientation->GetWidget(),
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
}

void PrintOptionsWidget::SpacingChanged()
{
    RefreshSizes();
}

void PrintOptionsWidget::BaseUnitChanged()
{
    const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
    const auto base_unit_name{ ToQString(g_Cfg.m_BaseUnit.m_ShortName) };

    const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
    const auto margins{ m_Project.ComputeMargins() / base_unit };

    m_LeftMarginSpin->setSuffix(base_unit_name);
    m_LeftMarginSpin->setRange(0, max_margins.x);
    m_LeftMarginSpin->setValue(margins.x);
    m_TopMarginSpin->setSuffix(base_unit_name);
    m_TopMarginSpin->setRange(0, max_margins.y);
    m_TopMarginSpin->setValue(margins.y);
    m_RightMarginSpin->setSuffix(base_unit_name);
    m_RightMarginSpin->setRange(0, max_margins.x);
    m_RightMarginSpin->setValue(margins.x);
    m_BottomMarginSpin->setSuffix(base_unit_name);
    m_BottomMarginSpin->setRange(0, max_margins.y);
    m_BottomMarginSpin->setValue(margins.y);

    // Update All Margins range and suffix
    m_AllMarginsSpin->setSuffix(base_unit_name);
    m_AllMarginsSpin->setRange(0, std::max(max_margins.x, max_margins.y));

    // Maintain enabled states based on margin mode
    const bool all_margins_mode{ m_MarginModeToggle->isChecked() };
    m_LeftMarginSpin->setEnabled(!all_margins_mode);
    m_TopMarginSpin->setEnabled(!all_margins_mode);
    m_RightMarginSpin->setEnabled(!all_margins_mode);
    m_BottomMarginSpin->setEnabled(!all_margins_mode);
    m_AllMarginsSpin->setEnabled(all_margins_mode);
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

void PrintOptionsWidget::SetDefaults()
{
    m_PrintOutput->setText(ToQString(m_Project.m_Data.m_FileName));
    m_CardSize->setCurrentText(ToQString(m_Project.m_Data.m_CardSizeChoice));
    m_PaperSize->setCurrentText(ToQString(m_Project.m_Data.m_PageSize));
    m_CardsWidth->setValue(m_Project.m_Data.m_CardLayout.x);
    m_CardsHeight->setValue(m_Project.m_Data.m_CardLayout.y);
    m_Orientation->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_Orientation)));
    m_FlipOn->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_FlipOn)));

    const bool infer_size{ m_Project.m_Data.m_PageSize == Config::c_BasePDFSize };
    m_BasePdf->GetWidget()->setCurrentText(ToQString(m_Project.m_Data.m_BasePdf));
    m_BasePdf->setEnabled(infer_size);
    m_BasePdf->setVisible(infer_size);

    const bool custom_margins{ m_Project.m_Data.m_CustomMargins.has_value() };
    const bool custom_margins_four{ m_Project.m_Data.m_CustomMarginsFour.has_value() };
    m_CustomMargins->setChecked(custom_margins || custom_margins_four);

    const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
    const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
    const auto margins{ m_Project.ComputeMargins() / base_unit };
    const auto margins_four{ m_Project.ComputeMarginsFour() };

    m_LeftMarginSpin->setRange(0, max_margins.x);
    m_LeftMarginSpin->setValue(custom_margins_four ? margins_four.left / base_unit : margins.x);
    m_LeftMarginSpin->setEnabled(custom_margins || custom_margins_four);

    m_TopMarginSpin->setRange(0, max_margins.y);
    m_TopMarginSpin->setValue(custom_margins_four ? margins_four.top / base_unit : margins.y);
    m_TopMarginSpin->setEnabled(custom_margins || custom_margins_four);

    m_RightMarginSpin->setRange(0, max_margins.x);
    m_RightMarginSpin->setValue(custom_margins_four ? margins_four.right / base_unit : margins.x);
    m_RightMarginSpin->setEnabled(custom_margins_four);

    m_BottomMarginSpin->setRange(0, max_margins.y);
    m_BottomMarginSpin->setValue(custom_margins_four ? margins_four.bottom / base_unit : margins.y);
    m_BottomMarginSpin->setEnabled(custom_margins_four);

    // Set up All Margins control
    m_AllMarginsSpin->setRange(0, std::max(max_margins.x, max_margins.y));
    m_AllMarginsSpin->setValue(m_LeftMarginSpin->value());
    m_AllMarginsSpin->setEnabled(custom_margins || custom_margins_four);

    // Set up margin mode toggle
    m_MarginModeToggle->setEnabled(custom_margins || custom_margins_four);
    m_MarginModeToggle->setChecked(false);

    // Set initial visibility and enabled states
    m_LeftMarginSpin->setVisible(true);
    m_TopMarginSpin->setVisible(true);
    m_RightMarginSpin->setVisible(true);
    m_BottomMarginSpin->setVisible(true);
    m_AllMarginsSpin->setVisible(true);

    // Initial enabled states (all margins mode starts disabled)
    m_LeftMarginSpin->setEnabled(true);
    m_TopMarginSpin->setEnabled(true);
    m_RightMarginSpin->setEnabled(true);
    m_BottomMarginSpin->setEnabled(true);
    m_AllMarginsSpin->setEnabled(false);
}

void PrintOptionsWidget::RefreshSizes()
{
    RefreshCardLayout();

    m_PaperInfo->setText(ToQString(SizeToString(m_Project.ComputePageSize())));
    m_CardsInfo->setText(ToQString(SizeToString(m_Project.ComputeCardsSize())));

    const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
    const auto max_margins{ m_Project.ComputeMaxMargins() / base_unit };
    const auto margins{ m_Project.ComputeMargins() / base_unit };
    const auto margins_four{ m_Project.ComputeMarginsFour() };

    // Preserve current margin values when updating ranges
    const auto current_left{ m_LeftMarginSpin->value() };
    const auto current_top{ m_TopMarginSpin->value() };
    const auto current_right{ m_RightMarginSpin->value() };
    const auto current_bottom{ m_BottomMarginSpin->value() };

    m_LeftMarginSpin->setRange(0, max_margins.x);
    m_LeftMarginSpin->setValue(current_left);

    m_TopMarginSpin->setRange(0, max_margins.y);
    m_TopMarginSpin->setValue(current_top);

    m_RightMarginSpin->setRange(0, max_margins.x);
    m_RightMarginSpin->setValue(current_right);

    m_BottomMarginSpin->setRange(0, max_margins.y);
    m_BottomMarginSpin->setValue(current_bottom);

    // Update All Margins range
    m_AllMarginsSpin->setRange(0, std::max(max_margins.x, max_margins.y));

    // Maintain enabled states based on margin mode
    const bool all_margins_mode{ m_MarginModeToggle->isChecked() };
    m_LeftMarginSpin->setEnabled(!all_margins_mode);
    m_TopMarginSpin->setEnabled(!all_margins_mode);
    m_RightMarginSpin->setEnabled(!all_margins_mode);
    m_BottomMarginSpin->setEnabled(!all_margins_mode);
    m_AllMarginsSpin->setEnabled(all_margins_mode);
}

void PrintOptionsWidget::RefreshCardLayout()
{
    if (m_Project.CacheCardLayout())
    {
        m_CardsWidth->setValue(m_Project.m_Data.m_CardLayout.x);
        m_CardsHeight->setValue(m_Project.m_Data.m_CardLayout.y);
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
    const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
    const auto base_unit_name{ g_Cfg.m_BaseUnit.m_ShortName };
    return fmt::format("{:.1f} x {:.1f} {}", size.x / base_unit, size.y / base_unit, base_unit_name);
}
