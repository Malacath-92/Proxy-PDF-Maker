#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include <QObject>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/constants.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

#include <ppp/project/card_info.hpp>

using CardContainer = std::vector<CardInfo>;
using CardSorting = std::vector<fs::path>;

struct ImagePreview
{
    Image m_UncroppedImage;
    Image m_CroppedImage;
    bool m_BadAspectRatio;
    bool m_BadRotation;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

enum class FlipPageOn
{
    LeftEdge,
    TopEdge,
};

enum class CardCorners
{
    Square,
    Rounded,
};

enum class MarginsMode
{
    Auto,
    Simple,
    Full,
    Linked,
};

template<class T>
struct GenericMargins
{
    T m_Left{};
    T m_Top{};
    T m_Right{};
    T m_Bottom{};

    template<class U>
    auto operator*(const U& rhs)
    {
        using ResT = decltype(std::declval<T>() * std::declval<U>());
        return GenericMargins<ResT>{
            m_Left * rhs,
            m_Top * rhs,
            m_Right * rhs,
            m_Bottom * rhs,
        };
    }
    template<class U>
    auto operator/(const U& rhs)
    {
        using ResT = decltype(std::declval<T>() / std::declval<U>());
        return GenericMargins<ResT>{
            m_Left / rhs,
            m_Top / rhs,
            m_Right / rhs,
            m_Bottom / rhs,
        };
    }
};
using Margins = GenericMargins<Length>;

// Individual margin controls allow for asymmetric layouts needed in professional printing
// where different margins are required for binding, cutting, or aesthetic purposes
struct CustomMargins
{
    Size m_TopLeft{ 0_mm, 0_mm };
    std::optional<Size> m_BottomRight{ std::nullopt };
};

enum CardOrientation
{
    Vertical,
    Horizontal,
    Mixed,
};

struct ProjectData
{
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
    fs::path m_BacksideDefault{ "__back.png" };
    Offset m_BacksideOffset{ 0_mm, 0_mm };
    Angle m_BacksideRotation{ 0_deg };
    std::string m_BacksideAutoPattern{ "__back_$" };

    // PDF generation options
    std::string m_CardSizeChoice{ g_Cfg.GetFirstValidCardSize() };
    std::string m_PageSize{ g_Cfg.GetFirstValidPageSize() };
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
    fs::path GetOutputFolder(const Config& config) const;

    struct CardLayout
    {
        dla::uvec2 m_CardLayoutVertical;
        dla::uvec2 m_CardLayoutHorizontal;
    };
    CardLayout ComputeAutoCardLayout(const Config& config, Size available_space) const;
    dla::uvec2 ComputeCardLayout(const Config& config,
                                 Size available_space,
                                 CardOrientation orientation) const;

    Size ComputePageSize(const Config& config) const;
    Size ComputeCardsSize(const Config& config) const;
    Size ComputeCardsSize(const Size& card_size_with_bleed, const dla::uvec2& card_layout) const;
    Margins ComputeMargins(const Config& config) const;
    Size ComputeMaxMargins(const Config& config) const;
    Size ComputeMaxMargins(const Config& config, MarginsMode margins_mode) const;
    Size ComputeDefaultMargins(const Config& config) const;

    const Config::CardSizeInfo& CardSizeInfo(const Config& config) const;
    float CardRatio(const Config& config) const;
    Size CardSize(const Config& config) const;
    Size CardSizeWithBleed(const Config& config) const;
    Size CardSizeWithFullBleed(const Config& config) const;
    Length CardFullBleed(const Config& config) const;

    bool IsCardRoundedRect(const Config& config) const;
    Length CardCornerRadius(const Config& config) const;

    bool IsCardSvg(const Config& config) const;
    const Svg& CardSvgData(const Config& config) const;
};

class JsonProvider;

class Project : public QObject
{
    Q_OBJECT

  public:
    Project() = default;
    ~Project();

    bool Load(const fs::path& json_path);
    bool Load(const fs::path& json_path,
              const JsonProvider* overrides);
    bool LoadFromJson(const std::string& json_blob,
                      const JsonProvider* overrides);

    void Dump(const fs::path& json_path) const;
    std::string DumpToJson() const;

    void Init();
    void InitProperties();

    fs::path GetOutputFolder() const;

    CardInfo& CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);
    void CardModified(const fs::path& card_name);

    bool HasCard(const fs::path& card_name) const;
    const CardInfo* FindCard(const fs::path& card_name) const;
    CardInfo* FindCard(const fs::path& card_name);

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

    bool HasNonDefaultBacksideImage(const fs::path& card_name) const;
    const fs::path& GetBacksideImage(const fs::path& card_name) const;
    bool SetBacksideImage(const fs::path& card_name, fs::path backside_image);
    bool SetBacksideImageDefault(const fs::path& card_name);

    bool HasCardBacksideShortEdge(const fs::path& card_name) const;
    void SetCardBacksideShortEdge(const fs::path& card_name, bool has_backside_short_edge);

    bool SetBacksideAutoPattern(std::string pattern);

    bool CacheCardLayout();

    Size ComputePageSize() const;
    Size ComputeCardsSize() const;
    Size ComputeCardsSizeVertical() const;
    Size ComputeCardsSizeHorizontal() const;
    Margins ComputeMargins() const;
    Size ComputeMaxMargins() const;
    Size ComputeDefaultMargins() const;

    void SetMarginsMode(MarginsMode margins_mode);

    bool SetBacksideEnabled(bool backside_enabled);

    float CardRatio() const;
    Size CardSize() const;
    Size CardSizeWithBleed() const;
    Size CardSizeWithFullBleed() const;
    Length CardFullBleed() const;

    bool IsCardRoundedRect() const;
    Length CardCornerRadius() const;

    bool IsCardSvg() const;
    const Svg& CardSvgData() const;

    void EnsureOutputFolder() const;

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

  signals:
    void FailedAddingExternalCard(const fs::path& absolute_image_path);
    void ExternalCardAdded(const fs::path& absolute_image_path);
    void ExternalCardRemoved(const fs::path& absolute_image_path);

    void BacksideEnabledChanged(bool backside_enabled);

    void PreviewRemoved(const fs::path& card_name);
    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);

    void CardVisibilityChanged(const fs::path& card_name, bool visible);
    void CardBacksideChanged(const fs::path& card_name, const fs::path& backside);
    void CardRotationChanged(const fs::path& card_name, Image::Rotation rotation);
    void CardBleedTypeChanged(const fs::path& card_name, BleedType bleed_type);
    void CardBadAspectRatioHandlingChanged(const fs::path& card_name, BadAspectRatioHandling ratio_handling);

  public:
    ProjectData m_Data;

  private:
    Project(const Project&) = delete;
    Project(Project&&) = delete;
    Project& operator=(const Project&) = delete;
    Project& operator=(Project&&) = delete;

    CardSorting GenerateDefaultCardsSorting() const;
    void AppendCardToList(const fs::path& card_name);
    void RemoveCardFromList(const fs::path& card_name);

    bool AutoMatchBackside(const fs::path& card_name);
    std::optional<fs::path> FindCardAutoBackside(const fs::path& card_name) const;
    std::optional<fs::path> MatchAsAutoBackside(const fs::path& card_name) const;
};
