#include <ppp/ui/widget_print_options.hpp>

#include <ranges>

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

    const auto initial_base_unit{ CFG.BaseUnit.m_Unit };
    const auto initial_base_unit_name{ ToQString(CFG.BaseUnit.m_ShortName) };

    const bool initial_fit_size{ project.Data.PageSize == Config::FitSize };
    const bool initial_infer_size{ project.Data.PageSize == Config::BasePDFSize };

    const auto initial_page_size{ project.ComputePageSize() };
    const auto initial_cards_size{ project.ComputeCardsSize() };
    const auto initial_max_margins{ (initial_page_size - initial_cards_size) / initial_base_unit };
    const auto initial_margins{ project.ComputeMargins() / initial_base_unit };

    using namespace std::string_view_literals;
    auto* print_output{ new LineEditWithLabel{ "Output &Filename", project.Data.FileName.string() } };
    m_PrintOutput = print_output->GetWidget();

    auto* card_size{ new ComboBoxWithLabel{
        "&Card Size", std::views::keys(CFG.CardSizes) | std::ranges::to<std::vector>(), project.Data.CardSizeChoice } };
    card_size->setToolTip("Additional card sizes can be defined in config.ini");
    m_CardSize = card_size->GetWidget();

    auto* paper_size{ new ComboBoxWithLabel{
        "&Paper Size", std::views::keys(CFG.PageSizes) | std::ranges::to<std::vector>(), project.Data.PageSize } };
    paper_size->setToolTip("Additional card sizes can be defined in config.ini");
    m_PaperSize = paper_size->GetWidget();

    m_BasePdf = new ComboBoxWithLabel{
        "&Base Pdf", GetBasePdfNames(), project.Data.BasePdf
    };
    m_BasePdf->setEnabled(initial_infer_size);
    m_BasePdf->setVisible(initial_infer_size);

    auto* paper_info{ new LabelWithLabel{ "", SizeToString(initial_page_size) } };
    m_PaperInfo = paper_info->GetWidget();

    auto* cards_info{ new LabelWithLabel{ "Cards Size", SizeToString(initial_cards_size) } };
    m_CardsInfo = cards_info->GetWidget();

    auto* left_margin{ new DoubleSpinBoxWithLabel{ "&Left Margin" } };
    m_LeftMarginSpin = left_margin->GetWidget();
    m_LeftMarginSpin->setDecimals(2);
    m_LeftMarginSpin->setSingleStep(0.1);
    m_LeftMarginSpin->setSuffix(initial_base_unit_name);
    m_LeftMarginSpin->setRange(0, initial_max_margins.x);
    m_LeftMarginSpin->setValue(initial_margins.x);

    auto* top_margin{ new DoubleSpinBoxWithLabel{ "&Top Margin" } };
    m_TopMarginSpin = top_margin->GetWidget();
    m_TopMarginSpin->setDecimals(2);
    m_TopMarginSpin->setSingleStep(0.1);
    m_TopMarginSpin->setSuffix(initial_base_unit_name);
    m_TopMarginSpin->setRange(0, initial_max_margins.y);
    m_TopMarginSpin->setValue(initial_margins.y);

    m_CardsWidth = new QDoubleSpinBox;
    m_CardsWidth->setDecimals(0);
    m_CardsWidth->setRange(1, 10);
    m_CardsWidth->setSingleStep(1);
    m_CardsWidth->setValue(project.Data.CardLayout.x);
    m_CardsHeight = new QDoubleSpinBox;
    m_CardsHeight->setDecimals(0);
    m_CardsHeight->setRange(1, 10);
    m_CardsHeight->setSingleStep(1);
    m_CardsHeight->setValue(project.Data.CardLayout.y);
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
        "&Orientation", magic_enum::enum_names<PageOrientation>(), magic_enum::enum_name(project.Data.Orientation) } };
    orientation->setEnabled(!initial_fit_size && !initial_infer_size);
    orientation->setVisible(!initial_fit_size && !initial_infer_size);
    m_Orientation = orientation->GetWidget();

    auto* layout{ new QVBoxLayout };
    layout->addWidget(print_output);
    layout->addWidget(card_size);
    layout->addWidget(paper_size);
    layout->addWidget(m_BasePdf);
    layout->addWidget(paper_info);
    layout->addWidget(cards_info);
    layout->addWidget(left_margin);
    layout->addWidget(top_margin);
    layout->addWidget(cards_layout);
    layout->addWidget(orientation);
    setLayout(layout);

    auto change_output{
        [=, this](QString t)
        {
            m_Project.Data.FileName = t.toStdString();
        }
    };

    auto change_papersize{
        [=, this](QString t)
        {
            m_Project.Data.PageSize = t.toStdString();

            const Size page_size{ m_Project.ComputePageSize() };

            const bool infer_size{ m_Project.Data.PageSize == Config::BasePDFSize };
            const bool fit_size{ m_Project.Data.PageSize == Config::FitSize };
            if (!fit_size)
            {
                const Size card_size_with_bleed{ m_Project.CardSizeWithBleed() };
                m_Project.Data.CardLayout = static_cast<dla::uvec2>(dla::floor(page_size / card_size_with_bleed));
            }

            const Size cards_size{ m_Project.ComputeCardsSize() };
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            const auto max_margins{ (page_size - cards_size) / base_unit };

            m_LeftMarginSpin->setRange(0, max_margins.x);
            m_LeftMarginSpin->setValue(max_margins.x / 2.0f);
            m_TopMarginSpin->setRange(0, max_margins.y);
            m_TopMarginSpin->setValue(max_margins.y / 2.0f);

            paper_info->GetWidget()->setText(ToQString(SizeToString(page_size)));
            cards_info->GetWidget()->setText(ToQString(SizeToString(cards_size)));

            m_BasePdf->setEnabled(infer_size);
            m_BasePdf->setVisible(infer_size);
            cards_layout->setEnabled(fit_size);
            cards_layout->setVisible(fit_size);
            orientation->setEnabled(!fit_size && !infer_size);
            orientation->setVisible(!fit_size && !infer_size);

            PageSizeChanged();
        }
    };

    auto change_cardsize{
        [=, this](QString t)
        {
            std::string new_choice{ t.toStdString() };
            if (new_choice == m_Project.Data.CardSizeChoice)
            {
                return;
            }

            m_Project.Data.CardSizeChoice = std::move(new_choice);
            CardSizeChanged();

            // Refresh anything needed for size change
            change_papersize(ToQString(m_Project.Data.PageSize));
        }
    };

    auto change_base_pdf{
        [=, this](QString t)
        {
            const bool infer_size{ m_Project.Data.PageSize == Config::BasePDFSize };
            if (!infer_size)
            {
                return;
            }

            m_Project.Data.BasePdf = t.toStdString();

            const Size page_size{ m_Project.ComputePageSize() };
            const Size card_size_with_bleed{ m_Project.CardSizeWithBleed() };
            m_Project.Data.CardLayout = static_cast<dla::uvec2>(dla::floor(page_size / card_size_with_bleed));

            const auto cards_size{ m_Project.ComputeCardsSize() };
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            const auto max_margins{ (page_size - cards_size) / base_unit };

            m_LeftMarginSpin->setRange(0, max_margins.x);
            m_LeftMarginSpin->setValue(max_margins.x / 2.0f);
            m_TopMarginSpin->setRange(0, max_margins.y);
            m_TopMarginSpin->setValue(max_margins.y / 2.0f);

            paper_info->GetWidget()->setText(ToQString(SizeToString(page_size)));
            cards_info->GetWidget()->setText(ToQString(SizeToString(cards_size)));

            PageSizeChanged();
        }
    };

    auto change_top_margin{
        [=, this](double v)
        {
            if (!m_Project.Data.CustomMargins.has_value())
            {
                m_Project.Data.CustomMargins.emplace(m_Project.ComputeMargins());
            }

            const auto base_unit{ CFG.BaseUnit.m_Unit };
            m_Project.Data.CustomMargins.value().y = static_cast<float>(v) * base_unit;
            MarginsChanged();
        }
    };

    auto change_left_margin{
        [=, this](double v)
        {
            if (!m_Project.Data.CustomMargins.has_value())
            {
                m_Project.Data.CustomMargins.emplace(m_Project.ComputeMargins());
            }
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            m_Project.Data.CustomMargins.value().x = static_cast<float>(v) * base_unit;
            MarginsChanged();
        }
    };

    auto change_cards_width{
        [=, this](double v)
        {
            m_Project.Data.CardLayout.x = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_cards_height{
        [=, this](double v)
        {
            m_Project.Data.CardLayout.y = static_cast<uint32_t>(v);
            CardLayoutChanged();
        }
    };

    auto change_orientation{
        [=, this](QString t)
        {
            m_Project.Data.Orientation = magic_enum::enum_cast<PageOrientation>(t.toStdString())
                                             .value_or(PageOrientation::Portrait);

            const bool fit_size{ m_Project.Data.PageSize == Config::FitSize };
            const bool infer_size{ m_Project.Data.PageSize == Config::BasePDFSize };
            if (fit_size || infer_size)
            {
                return;
            }

            const Size page_size{ m_Project.ComputePageSize() };
            const Size card_size_with_bleed{ m_Project.CardSizeWithBleed() };
            m_Project.Data.CardLayout = static_cast<dla::uvec2>(dla::floor(page_size / card_size_with_bleed));

            const auto cards_size{ m_Project.ComputeCardsSize() };
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            const auto max_margins{ (page_size - cards_size) / base_unit };

            m_LeftMarginSpin->setRange(0, max_margins.x);
            m_LeftMarginSpin->setValue(max_margins.x / 2.0f);
            m_TopMarginSpin->setRange(0, max_margins.y);
            m_TopMarginSpin->setValue(max_margins.y / 2.0f);

            OrientationChanged();
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
    QObject::connect(m_LeftMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_left_margin);
    QObject::connect(m_TopMarginSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_top_margin);
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
}

void PrintOptionsWidget::NewProjectOpened()
{
    SetDefaults();
}

void PrintOptionsWidget::BleedChanged()
{
    RefreshSizes();
}

void PrintOptionsWidget::BaseUnitChanged()
{
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto base_unit_name{ ToQString(CFG.BaseUnit.m_ShortName) };

    const auto page_size{ m_Project.ComputePageSize() };
    const auto cards_size{ m_Project.ComputeCardsSize() };
    const auto max_margins{ page_size - cards_size };
    const auto margins{ m_Project.ComputeMargins() };

    m_LeftMarginSpin->setSuffix(base_unit_name);
    m_LeftMarginSpin->setRange(0, max_margins.x / base_unit);
    m_LeftMarginSpin->setValue(margins.x / base_unit);
    m_TopMarginSpin->setSuffix(base_unit_name);
    m_TopMarginSpin->setRange(0, max_margins.y / base_unit);
    m_TopMarginSpin->setValue(margins.y / base_unit);
}

void PrintOptionsWidget::RenderBackendChanged()
{
    const int base_pdf_size_idx{
        [&]()
        {
            for (int i = 0; i < m_PaperSize->count(); i++)
            {
                if (m_PaperSize->itemText(i).toStdString() == Config::BasePDFSize)
                {
                    return i;
                }
            }
            return -1;
        }()
    };
    const bool has_base_pdf_option{ base_pdf_size_idx >= 0 };
    const bool has_base_pdf_confg{ CFG.PageSizes.contains(std::string{ Config::BasePDFSize }) };
    const bool need_add_base_pdf_option{ !has_base_pdf_option && has_base_pdf_confg };
    const bool need_remove_base_pdf_option{ has_base_pdf_option && !has_base_pdf_confg };

    if (need_add_base_pdf_option || need_remove_base_pdf_option)
    {
        if (need_add_base_pdf_option)
        {
            m_PaperSize->addItem(ToQString(Config::BasePDFSize));
        }
        else
        {
            m_PaperSize->removeItem(base_pdf_size_idx);
        }
        m_PaperSize->setCurrentText(ToQString(m_Project.Data.PageSize));
    }

    if (CFG.Backend != PdfBackend::PoDoFo && m_Project.Data.PageSize == Config::BasePDFSize)
    {
        m_PaperSize->setCurrentText(ToQString(CFG.DefaultPageSize));
        m_Project.Data.PageSize = CFG.DefaultPageSize;
        PageSizeChanged();
    }
}

void PrintOptionsWidget::SetDefaults()
{
    m_PrintOutput->setText(ToQString(m_Project.Data.FileName));
    m_CardSize->setCurrentText(ToQString(m_Project.Data.CardSizeChoice));
    m_PaperSize->setCurrentText(ToQString(m_Project.Data.PageSize));
    m_CardsWidth->setValue(m_Project.Data.CardLayout.x);
    m_CardsHeight->setValue(m_Project.Data.CardLayout.y);
    m_Orientation->setCurrentText(ToQString(magic_enum::enum_name(m_Project.Data.Orientation)));

    const bool infer_size{ m_Project.Data.PageSize == Config::BasePDFSize };
    m_BasePdf->GetWidget()->setCurrentText(ToQString(m_Project.Data.BasePdf));
    m_BasePdf->setEnabled(infer_size);
    m_BasePdf->setVisible(infer_size);

    RefreshSizes();
}

void PrintOptionsWidget::RefreshSizes()
{
    m_PaperInfo->setText(ToQString(SizeToString(m_Project.ComputePageSize())));
    m_CardsInfo->setText(ToQString(SizeToString(m_Project.ComputeCardsSize())));

    const auto page_size{ m_Project.ComputePageSize() };
    const auto cards_size{ m_Project.ComputeCardsSize() };
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto max_margins{ page_size - cards_size };
    m_LeftMarginSpin->setValue(max_margins.x / base_unit / 2.0f);
    m_TopMarginSpin->setValue(max_margins.y / base_unit / 2.0f);
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
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto base_unit_name{ CFG.BaseUnit.m_ShortName };
    return fmt::format("{:.1f} x {:.1f} {}", size.x / base_unit, size.y / base_unit, base_unit_name);
}
