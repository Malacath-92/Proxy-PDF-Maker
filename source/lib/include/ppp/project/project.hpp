#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include <QObject>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/config_types.hpp>
#include <ppp/constants.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

#include <ppp/project/card_info.hpp>
#include <ppp/project/project_types.hpp>

struct ProjectData
{
    ProjectData(const std::string_view default_card_size_choice,
                const std::string_view default_page_size_choice);
    ProjectData(const Config& config);

    // Project options
    fs::path m_ImageDir{ "images" };
    fs::path m_CropDir{ "images/crop" };
    fs::path m_UncropDir{ "images/uncrop" };
    fs::path m_ImageCache{ "images/crop/preview.cache" };

    // List of all cards
    CardContainer m_Cards{};
    ImgDict m_Previews{};
    ImagePreview m_FallbackPreview{};

    // Possibly empty list of all cards, determines user-provided order of cards
    // or if empty implies automatic ordering
    CardSorting m_CardsList{};

    // Card options
    Length m_BleedEdge{ 0_mm };
    Length m_EnvelopeBleedEdge{ 0_mm };
    Size m_Spacing{ 0_mm, 0_mm };
    bool m_SpacingLinked{ true };
    CardCorners m_Corners{ CardCorners::Square };

    // Backside options
    bool m_BacksideEnabled{ false };
    bool m_SeparateBacksides{ false };
    std::optional<fs::path> m_BacksideDefault{ "__back.png" };
    Offset m_BacksideOffset{ 0_mm, 0_mm };
    Angle m_BacksideRotation{ 0_deg };
    Length m_BacksideExtraBleedEdge{ 0_mm };
    std::string m_BacksideAutoPattern{ "__back_$" };

    // PDF generation options
    std::string m_CardSizeChoice;
    std::string m_PageSize;
    std::string m_BasePdf{ "None" };

    // Margin mode is the user-selected edit-mode of margins
    MarginsMode m_MarginsMode{ MarginsMode::Auto };

    // Custom margins provide fine-grained control over page layout for professional printing
    // where standard centered margins may not meet specific requirements
    std::optional<CustomMargins> m_CustomMargins{};

    // The way cards are layed out on the card can help maximize the amount of cards or
    // reduce print errors
    CardOrientation m_CardOrientation{ CardOrientation::Vertical };
    dla::uvec2 m_CardLayoutVertical{ 3, 3 };
    dla::uvec2 m_CardLayoutHorizontal{};
    std::vector<size_t> m_SkippedLayoutSlots{};

    PageOrientation m_Orientation{ PageOrientation::Portrait };
    FlipPageOn m_FlipOn{ FlipPageOn::LeftEdge };
    fs::path m_FileName{ "_printme" };
    bool m_RenderPageHeader{ true };

    // Guides options
    bool m_ExportExactGuides{ false };
    bool m_EnableGuides{ true };
    bool m_BacksideEnableGuides{ false };
    bool m_CornerGuides{ true };
    bool m_CrossGuides{ false };
    bool m_ExtendedGuides{ true };
    ColorRGB8 m_GuidesColorA{ 0, 0, 0 };
    ColorRGB8 m_GuidesColorB{ 190, 190, 190 };
    Length m_GuidesOffset{ 0_mm };
    Length m_GuidesThickness{ 1_pts };
    Length m_GuidesLength{ 1.5_mm };

    // Utility functions
    fs::path GetOutputFolder(const ConfigData& config) const;
    fs::path GetBacksideOutputFolder(const ConfigData& config) const;

    struct CardLayout
    {
        dla::uvec2 m_CardLayoutVertical;
        dla::uvec2 m_CardLayoutHorizontal;
    };
    CardLayout ComputeAutoCardLayout(const ConfigData& config, Size available_space) const;
    dla::uvec2 ComputeCardLayout(const ConfigData& config,
                                 Size available_space,
                                 CardOrientation orientation) const;

    Size ComputePageSize(const ConfigData& config) const;
    Size ComputeExactBordersSize(const ConfigData& config) const;
    Size ComputeCardsSize(const ConfigData& config) const;
    Size ComputeCardsSize(const Size& card_size_with_bleed, const dla::uvec2& card_layout) const;
    Margins ComputeMargins(const ConfigData& config) const;
    Size ComputeMaxMargins(const ConfigData& config) const;
    Size ComputeMaxMargins(const ConfigData& config, MarginsMode margins_mode) const;
    Size ComputeDefaultMargins(const ConfigData& config) const;

    const ::CardSizeInfo& CardSizeInfo(const ConfigData& config) const;
    float CardRatio(const ConfigData& config) const;
    Size CardSize(const ConfigData& config) const;
    Size CardSizeWithBleed(const ConfigData& config) const;
    Size CardSizeWithFullBleed(const ConfigData& config) const;
    Length CardFullBleed(const ConfigData& config) const;

    bool IsCardRoundedRect(const ConfigData& config) const;
    Length CardCornerRadius(const ConfigData& config) const;

    bool IsCardSvg(const ConfigData& config) const;
    const Svg& CardSvgData(const ConfigData& config) const;

    const CardInfo* FindCard(const fs::path& card_name) const;
    CardInfo* FindCard(const fs::path& card_name);

    CardSorting GenerateDefaultCardsSorting() const;
};

class JsonProvider;

class Project : public QObject
{
    Q_OBJECT

  public:
    Project(const Config& config);
    ~Project();

    bool Load(const fs::path& json_path);
    bool Load(const fs::path& json_path,
              const JsonProvider* overrides);
    bool LoadFromJson(const std::string& json_blob,
                      const JsonProvider* overrides);

    void Dump(const fs::path& json_path) const;
    std::string DumpToJson() const;
    static std::string DumpToJson(const ProjectData& data);

    void Init();
    void InitProperties();

    fs::path GetOutputFolder() const;
    fs::path GetBacksideOutputFolder() const;

    CardInfo& CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

    bool HasCard(const fs::path& card_name) const;
    const CardInfo* FindCard(const fs::path& card_name) const;
    CardInfo* FindCard(const fs::path& card_name);

    bool HasCardByStem(const fs::path& card_name) const;
    const CardInfo* FindCardByStem(const fs::path& card_name) const;
    CardInfo* FindCardByStem(const fs::path& card_name);

    CardInfo& PutCard(const fs::path& card_name);
    CardInfo& PutCard(CardInfo card);
    std::optional<CardInfo> EatCard(const fs::path& card_name);

    bool HasExternalCards() const;

    fs::path GetCardImagePath(const fs::path& card_name) const;
    bool IsCardExternal(const fs::path& card_name) const;

    bool HideCard(const fs::path& card_name);
    bool UnhideCard(const fs::path& card_name);

    Image::Rotation GetCardRotation(const fs::path& card_name) const;
    bool RotateCardLeft(const fs::path& card_name);
    bool RotateCardRight(const fs::path& card_name);

    BleedType GetCardBleedType(const fs::path& card_name) const;
    bool SetCardBleedType(const fs::path& card_name, BleedType bleed_type);

    BadAspectRatioHandling GetCardBadAspectRatioHandling(const fs::path& card_name) const;
    bool SetCardBadAspectRatioHandling(const fs::path& card_name, BadAspectRatioHandling ratio_handling);

    uint32_t GetCardCount(const fs::path& card_name) const;
    uint32_t SetCardCount(const fs::path& card_name, uint32_t num);
    uint32_t IncrementCardCount(const fs::path& card_name);
    uint32_t DecrementCardCount(const fs::path& card_name);

    void CardOrderChanged();
    void CardOrderDirectionChanged();

    void RestoreCardsOrder();
    bool ReorderCards(size_t from, size_t to);

    bool HasPreview(const fs::path& card_name) const;
    bool HasBadAspectRatio(const fs::path& card_name) const;
    bool HasBadRotation(const fs::path& card_name) const;
    const ImagePreview& GetPreview(const fs::path& card_name) const;
    const Image& GetCroppedPreview(const fs::path& card_name) const;
    const Image& GetUncroppedPreview(const fs::path& card_name) const;
    const Image& GetCroppedBacksidePreview(const fs::path& card_name) const;
    const Image& GetUncroppedBacksidePreview(const fs::path& card_name) const;

    void SetOutputFilename(fs::path output_filename);
    void SetPageHeaderEnabled(bool page_header_enabled);

    void SetCardSizeChoice(std::string card_size_choice);
    void SetCardOrientation(CardOrientation card_orientation);
    void SetPageSizeChoice(std::string page_size_choice);
    void SetBasePdf(std::string base_pdf);
    void SetPageOrientation(PageOrientation page_orientation);
    void SetFlipPageOn(FlipPageOn flip_page_on);

    void SetPageMarginsMode(MarginsMode margins_mode);
    void SetPageMargin(Margin margin, Length margin_value);
    void SetCardsLayoutVertical(dla::uvec2 cards_layout);
    void SetCardsLayoutHorizontal(dla::uvec2 cards_layout);

    void SetExportExactGuides(bool export_exact_guides);
    void SetGuidesEnabled(bool guides_enabled);
    void SetBacksideGuidesEnabled(bool backside_guides_enabled);
    void SetCornerGuidesEnabled(bool corner_guides_enabled);
    void SetCrossGuidesEnabled(bool cross_guides_enabled);
    void SetExtendedGuidesEnabled(bool extended_guides_enabled);
    void SetGuidesColorA(ColorRGB8 guides_color);
    void SetGuidesColorB(ColorRGB8 guides_color);
    void SetGuidesOffset(Length guides_offset);
    void SetGuidesLength(Length guides_length);
    void SetGuidesThickness(Length guides_thickness);

    bool SetBacksideEnabled(bool backside_enabled);
    void SetSeparateBacksidesEnabled(bool separate_backsides);

    bool HasValidDefaultBackside() const;
    void SetBacksideDefault(const fs::path& backside_card_name);
    void ClearBacksideDefault();

    void SetBacksideOffset(Size offset);
    void SetBacksideRotation(Angle backside_rotation);
    void SetBacksideExtraBleedEdge(Length backside_extra_bleed_edge);

    bool HasClearBacksideImage(const fs::path& card_name) const;
    bool HasNonDefaultBacksideImage(const fs::path& card_name) const;
    OptionalImageRef GetBacksideImage(const fs::path& card_name) const;
    bool SetBacksideImage(const fs::path& card_name, fs::path backside_image);
    bool SetBacksideImageDefault(const fs::path& card_name);
    bool ClearBacksideImage(const fs::path& card_name);

    bool HasCardBacksideShortEdge(const fs::path& card_name) const;
    void SetCardBacksideShortEdge(const fs::path& card_name, bool has_backside_short_edge);

    bool SetBacksideAutoPattern(std::string pattern);

    bool CacheCardLayout();

    Size ComputePageSize() const;
    Size ComputeExactBordersSize() const;
    Size ComputeCardsSize() const;
    Size ComputeCardsSizeVertical() const;
    Size ComputeCardsSizeHorizontal() const;
    Margins ComputeMargins() const;
    Size ComputeMaxMargins() const;
    Size ComputeDefaultMargins() const;

    void SetMarginsMode(MarginsMode margins_mode);

    float CardRatio() const;
    Size CardSize() const;
    Size CardSizeWithBleed() const;
    Size CardSizeWithFullBleed() const;
    Length CardFullBleed() const;

    bool IsCardRoundedRect() const;
    Length CardCornerRadius() const;

    bool IsCardSvg() const;
    const Svg& CardSvgData() const;

    void SetImageDir(fs::path new_image_dir);
    void EnsureOutputFolder() const;

    void SetBleedEdge(Length bleed_edge);
    void SetEnvelopeBleedEdge(Length envelope_bleed_edge);

    void SetSpacing(Size spacing);
    void SetSpacingLinked(bool spacing_linked);

    void SetCorners(CardCorners corners);

    const auto& GetCards() const
    {
        return m_Data.m_Cards;
    }

    bool IsManuallySorted() const
    {
        return !m_Data.m_CardsList.empty();
    }
    auto GetManualSorting() const
    {
        return m_Data.m_CardsList;
    }

  public slots:
    void SetPreview(const fs::path& card_name,
                    ImagePreview preview,
                    Image::Rotation rotation);

    void CropperDone();

    bool AddExternalCard(const fs::path& absolute_image_path);
    bool RemoveExternalCard(const fs::path& card_name);

    void AvailableCardSizesChanged(const CardSizes& card_sizes);
    void AvailablePageSizesChanged(const PageSizes& page_sizes);

  signals:
    void FailedAddingExternalCard(const fs::path& absolute_image_path);
    void ExternalCardAdded(const fs::path& absolute_image_path);
    void ExternalCardRemoved(const fs::path& absolute_image_path);

    void OutputFilenameChanged(const fs::path& output_filename);
    void PageHeaderEnabledChanged(bool page_header_enabled);

    void CardSizeChoiceChanged(std::string card_size_choice);
    void CardSizeChanged(Size card_size);
    void CardOrientationChanged(CardOrientation card_orientation);
    void PageSizeChoiceChanged(std::string_view page_size_choice);
    void BasePdfChanged(std::string_view base_pdf);
    void PageSizeChanged(Size page_size);
    void PageOrientationChanged(PageOrientation page_orientation);
    void FlipPageOnChanged(FlipPageOn flip_page_on);

    void PageMarginsModeChanged(MarginsMode margins_mode);
    void PageMarginsChanged(Margins margins);
    void MaxPageMarginsChanged(Size margins);
    void CardsLayoutVerticalChanged(dla::uvec2 cards_layout);
    void CardsLayoutHorizontalChanged(dla::uvec2 cards_layout);

    void CardsSizeChanged(Size cards_size);

    void ExportExactGuidesChanged(bool export_exact_guides);
    void GuidesEnabledChanged(bool guides_enabled);
    void BacksideGuidesEnabledChanged(bool backside_guides_enabled);
    void CornerGuidesEnabledChanged(bool corner_guides_enabled);
    void CrossGuidesEnabledChanged(bool cross_guides_enabled);
    void ExtendedGuidesEnabledChanged(bool extended_guides_enabled);
    void GuidesColorAChanged(ColorRGB8 guides_color);
    void GuidesColorBChanged(ColorRGB8 guides_color);
    void GuidesOffsetChanged(Length guides_offset);
    void GuidesLengthChanged(Length guides_length);
    void GuidesThicknessChanged(Length guides_thickness);

    void BacksideEnabledChanged(bool backside_enabled);
    void SeparateBacksidesEnabledChanged(bool separate_backsides);

    void BacksideDefaultChanged(OptionalImageRef backside_card_name);

    void BacksideOffsetChanged(Size offset);
    void BacksideRotationChanged(Angle backside_rotation);
    void BacksideExtraBleedEdgeChanged(Length backside_extra_bleed_edge);

    void BacksideAutoPatternChanged(const std::string& pattern);

    void BleedEdgeChanged(Length bleed_edge);
    void EnvelopeBleedEdgeChanged(Length envelope_bleed_edge);

    void SpacingChanged(Size spacing);
    void SpacingLinkedChanged(bool spacing_linked);

    void CornersChanged(CardCorners corners);

    void PreviewRemoved(const fs::path& card_name);
    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);

    void CardVisibilityChanged(const fs::path& card_name, bool visible);
    void CardBacksideChanged(const fs::path& card_name, OptionalImageRef backside);
    void CardRotationChanged(const fs::path& card_name, Image::Rotation rotation);
    void CardBleedTypeChanged(const fs::path& card_name, BleedType bleed_type);
    void CardBadAspectRatioHandlingChanged(const fs::path& card_name, BadAspectRatioHandling ratio_handling);

    void ImageDirChanged(const fs::path& old_path, const fs::path& new_path);

  public:
    ProjectData m_Data;
    const Config& m_Cfg;

  private:
    Project(const Project&) = delete;
    Project(Project&&) = delete;
    Project& operator=(const Project&) = delete;
    Project& operator=(Project&&) = delete;

    void AppendCardToList(const fs::path& card_name);
    void RemoveCardFromList(const fs::path& card_name);

    bool AutoMatchBackside(const fs::path& card_name);
    std::optional<fs::path> FindCardAutoBackside(const fs::path& card_name) const;
    std::optional<fs::path> MatchAsAutoBackside(const fs::path& card_name) const;
};
