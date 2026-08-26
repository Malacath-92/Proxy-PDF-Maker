#include <ppp/ui/view_models/options/view_model_print_options.hpp>

#include <ranges>

#include <QDirIterator>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/pdf/generate.hpp>

#include <ppp/ui/default_project_value_actions.hpp>

#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

PrintOptionsViewModel::PrintOptionsViewModel(Project& project,
                                             Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Print Options");
}

void PrintOptionsViewModel::NewProjectOpened()
{
    EmitDefaults();
}

void PrintOptionsViewModel::BasePdfAdded()
{
    const auto available_base_pdfs{ GetBasePdfNames() };
    AvailableBasePdfsChanged(available_base_pdfs);
}

bool PrintOptionsViewModel::DoRenderAlignmentTest() const
{
    TRACY_AUTO_SCOPE();

    try
    {
        const auto file_path{ GenerateTestPdf(m_Project, m_Cfg) };
        OpenFile(file_path);
    }
    catch (const std::exception& e)
    {
        LogError("Failed creating render alignment test pdf: {}", e.what());
        return false;
    }

    return true;
}

void PrintOptionsViewModel::ChangeCardSizes(const CardSizes& card_sizes)
{
    m_Cfg.SetAvailableCardSizes(card_sizes);
}
void PrintOptionsViewModel::ChangePageSizes(const PageSizes& page_sizes)
{
    m_Cfg.SetAvailablePageSizes(page_sizes);
}

void PrintOptionsViewModel::ChangeOutputFilename(QString output_filename)
{
    m_Project.SetOutputFilename(output_filename.toStdString());
}
void PrintOptionsViewModel::ChangePageHeaderEnabled(Qt::CheckState page_header_enabled)
{
    m_Project.SetPageHeaderEnabled(page_header_enabled != Qt::CheckState::Unchecked);
}
void PrintOptionsViewModel::ChangeCardSizeChoice(QString card_size_choice)
{
    m_Project.SetCardSizeChoice(card_size_choice.toStdString());
}
void PrintOptionsViewModel::ChangePageSizeChoice(QString page_size_choice)
{
    m_Project.SetPageSizeChoice(page_size_choice.toStdString());
}
void PrintOptionsViewModel::ChangeBasePdf(QString base_pdf)
{
    m_Project.SetBasePdf(base_pdf.toStdString());
}
void PrintOptionsViewModel::ChangePageMarginsMode(QString margins_mode)
{
    m_Project.SetPageMarginsMode(magic_enum::enum_cast<MarginsMode>(
                                     margins_mode.toStdString())
                                     .value_or(MarginsMode::Auto));
}
void PrintOptionsViewModel::ChangePageMargin(Margin margin, Length value)
{
    m_Project.SetPageMargin(margin, value);
}
void PrintOptionsViewModel::ChangeCardOrientation(QString card_orientation)
{
    m_Project.SetCardOrientation(magic_enum::enum_cast<CardOrientation>(
                                     card_orientation.toStdString())
                                     .value_or(CardOrientation::Vertical));
}
void PrintOptionsViewModel::ChangeCardsLayoutVertical(double width, double height)
{
    m_Project.SetCardsLayoutVertical({ static_cast<uint32_t>(width),
                                       static_cast<uint32_t>(height) });
}
void PrintOptionsViewModel::ChangeCardsLayoutHorizontal(double width, double height)
{
    m_Project.SetCardsLayoutHorizontal({ static_cast<uint32_t>(width),
                                         static_cast<uint32_t>(height) });
}
void PrintOptionsViewModel::ChangePageOrientation(QString page_orientation)
{
    m_Project.SetPageOrientation(magic_enum::enum_cast<PageOrientation>(
                                     page_orientation.toStdString())
                                     .value_or(PageOrientation::Portrait));
}
void PrintOptionsViewModel::ChangeFlipPageOn(QString flip_on)
{
    m_Project.SetFlipPageOn(magic_enum::enum_cast<FlipPageOn>(
                                flip_on.toStdString())
                                .value_or(FlipPageOn::LeftEdge));
}

void PrintOptionsViewModel::EmitDefaults()
{
    AdvancedModeChanged(m_Cfg.m_AdvancedMode);
    BaseUnitChanged(m_Cfg.m_BaseUnit);
    AvailableCardSizesChanged(m_Cfg.m_CardSizes);
    AvailablePageSizesChanged(m_Cfg.m_PageSizes);
    AvailableBasePdfsChanged(GetBasePdfNames());

    OutputFilenameChanged(m_Project.m_Data.m_FileName);
    PageHeaderEnabledChanged(m_Project.m_Data.m_RenderPageHeader);
    CardSizeChoiceChanged(m_Project.m_Data.m_CardSizeChoice);
    PageSizeChanged(m_Project.ComputePageSize());
    PageSizeChoiceChanged(m_Project.m_Data.m_PageSize);
    BasePdfChanged(m_Project.m_Data.m_BasePdf);
    CardsSizeChanged(m_Project.ComputeCardsSize());
    PageMarginsModeChanged(m_Project.m_Data.m_MarginsMode);
    PageMarginsChanged(m_Project.ComputeMargins());
    CardOrientationChanged(m_Project.m_Data.m_CardOrientation);
    CardsLayoutVerticalChanged(m_Project.m_Data.m_CardLayoutVertical);
    CardsLayoutHorizontalChanged(m_Project.m_Data.m_CardLayoutHorizontal);
    PageOrientationChanged(m_Project.m_Data.m_Orientation);
    FlipPageOnChanged(m_Project.m_Data.m_FlipOn);
}

DefaultDataRequirements PrintOptionsViewModel::GetDefaultDataRequirements() const
{
    return DefaultDataRequirements{
        std::string{ m_Cfg.GetFirstValidCardSize() },
        std::string{ m_Cfg.GetFirstValidPageSize() },
    };
}
bool PrintOptionsViewModel::GetAdvancedMode() const
{
    return m_Cfg.m_AdvancedMode;
}
Unit PrintOptionsViewModel::GetBaseUnit() const
{
    return m_Cfg.m_BaseUnit;
}

const CardSizes& PrintOptionsViewModel::GetCardSizes() const
{
    return m_Cfg.m_CardSizes;
}
const CardSizes& PrintOptionsViewModel::GetDefaultCardSizes() const
{
    return Config::g_DefaultCardSizes;
}

const PageSizes& PrintOptionsViewModel::GetPageSizes() const
{
    return m_Cfg.m_PageSizes;
}
const PageSizes& PrintOptionsViewModel::GetDefaultPageSizes() const
{
    return Config::g_DefaultPageSizes;
}

std::vector<std::string> PrintOptionsViewModel::GetBasePdfNames() const
{
    TRACY_AUTO_SCOPE();

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

Size PrintOptionsViewModel::GetCardsSize() const
{
    return m_Project.ComputeCardsSize();
}
std::string_view PrintOptionsViewModel::GetPageSizeChoice() const
{
    return m_Project.m_Data.m_PageSize;
}
Size PrintOptionsViewModel::GetPageSize() const
{
    return m_Project.ComputePageSize();
}
CardOrientation PrintOptionsViewModel::GetCardOrientation() const
{
    return m_Project.m_Data.m_CardOrientation;
}
