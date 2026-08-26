#pragma once

#include <QObject>

#include <ppp/config.hpp>
#include <ppp/config_types.hpp>

#include <ppp/project/project_types.hpp>

class Project;
class Config;

struct DefaultDataRequirements;

class PrintOptionsViewModel : public QObject
{
    Q_OBJECT

    friend class PrintOptionsWidget;

  public:
    PrintOptionsViewModel(Project& project,
                          Config& config);

  signals:
    // forward

    void AdvancedModeChanged(bool advanced_mode);
    void BaseUnitChanged(Unit base_unit);
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
    void CardOrientationChanged(CardOrientation card_orientation);
    void CardsLayoutVerticalChanged(dla::uvec2 card_layout);
    void CardsLayoutHorizontalChanged(dla::uvec2 card_layout);
    void PageOrientationChanged(PageOrientation page_orientation);
    void FlipPageOnChanged(FlipPageOn flip_on);

  public slots:
    void NewProjectOpened();

    void BasePdfAdded();

  private slots:
    bool DoRenderAlignmentTest() const;

    void ChangeCardSizes(const CardSizes& card_sizes);
    void ChangePageSizes(const PageSizes& page_sizes);

    void ChangeOutputFilename(QString output_filename);
    void ChangePageHeaderEnabled(Qt::CheckState page_header_enabled);
    void ChangeCardSizeChoice(QString card_size_choice);
    void ChangePageSizeChoice(QString page_size_choice);
    void ChangeBasePdf(QString base_pdf);
    void ChangePageMarginsMode(QString margins_mode);
    void ChangePageMargin(Margin margin, Length value);
    void ChangeCardOrientation(QString card_orientation);
    void ChangeCardsLayoutVertical(double width, double height);
    void ChangeCardsLayoutHorizontal(double width, double height);
    void ChangePageOrientation(QString page_orientation);
    void ChangeFlipPageOn(QString flip_on);

  private:
    void EmitDefaults();

    DefaultDataRequirements GetDefaultDataRequirements() const;
    bool GetAdvancedMode() const;
    Unit GetBaseUnit() const;

    const CardSizes& GetCardSizes() const;
    const CardSizes& GetDefaultCardSizes() const;

    const PageSizes& GetPageSizes() const;
    const PageSizes& GetDefaultPageSizes() const;

    std::vector<std::string_view> GetAvailablePageSizes() const;
    std::vector<std::string> GetBasePdfNames() const;

    Size GetCardsSize() const;
    std::string_view GetPageSizeChoice() const;
    Size GetPageSize() const;
    CardOrientation GetCardOrientation() const;

    Project& m_Project;
    Config& m_Cfg;
};
