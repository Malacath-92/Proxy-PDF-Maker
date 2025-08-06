#pragma once

#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

#include <QObject>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/constants.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

struct CardInfo
{
    uint32_t m_Num{ 1 };
    uint32_t m_Hidden{ 0 };
    fs::path m_Backside{};
    bool m_BacksideShortEdge{ false };

    bool m_Transient{ false };
};
using CardMap = std::map<fs::path, CardInfo>;

struct ImagePreview
{
    Image m_UncroppedImage;
    Image m_CroppedImage;
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

class Project : public QObject
{
    Q_OBJECT

  public:
    Project() = default;
    ~Project();

    void Load(const fs::path& json_path);
    void Dump(const fs::path& json_path) const;

    void Init();
    void InitProperties();

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    bool HasPreview(const fs::path& image_name) const;
    const Image& GetCroppedPreview(const fs::path& image_name) const;
    const Image& GetUncroppedPreview(const fs::path& image_name) const;
    const Image& GetCroppedBacksidePreview(const fs::path& image_name) const;
    const Image& GetUncroppedBacksidePreview(const fs::path& image_name) const;

    const fs::path& GetBacksideImage(const fs::path& image_name) const;

    bool CacheCardLayout();

    Size ComputePageSize() const;
    Size ComputeCardsSize() const;
    Size ComputeMargins() const;
    Size ComputeMaxMargins() const;

    float CardRatio() const;
    Size CardSize() const;
    Size CardSizeWithBleed() const;
    Size CardSizeWithFullBleed() const;
    Length CardFullBleed() const;
    Length CardCornerRadius() const;

    void EnsureOutputFolder() const;

  public slots:
    void SetPreview(const fs::path& image_name, ImagePreview preview);

    void CropperDone();

  signals:
    void PreviewUpdated(const fs::path& image_name, const ImagePreview& preview);

  public:
    struct ProjectData
    {
        // Project options
        fs::path m_ImageDir{ "images" };
        fs::path m_CropDir{ "images/crop" };
        fs::path m_ImageCache{ "images/crop/preview.cache" };

        // List of all cards
        CardMap m_Cards{};
        ImgDict m_Previews{};
        ImagePreview m_FallbackPreview{};

        // Card options
        Length m_BleedEdge{ 0_mm };
        Size m_Spacing{ 0_mm, 0_mm };
        bool m_SpacingLinked{ true };
        CardCorners m_Corners{ CardCorners::Square };

        // Backside options
        bool m_BacksideEnabled{ false };
        fs::path m_BacksideDefault{ "__back.png" };
        Length m_BacksideOffset{ 0_mm };

        // PDF generation options
        std::string m_CardSizeChoice{ g_Cfg.m_DefaultCardSize };
        std::string m_PageSize{ g_Cfg.m_DefaultPageSize };
        std::string m_BasePdf{ "None" };
        std::optional<Size> m_CustomMargins{};
        dla::uvec2 m_CardLayout{ 3, 3 };
        PageOrientation m_Orientation{ PageOrientation::Portrait };
        FlipPageOn m_FlipOn{ FlipPageOn::LeftEdge };
        fs::path m_FileName{ "_printme" };

        // Guides options
        bool m_ExportExactGuides{ false };
        bool m_EnableGuides{ true };
        bool m_BacksideEnableGuides{ false };
        bool m_CornerGuides{ true };
        bool m_CrossGuides{ false };
        bool m_ExtendedGuides{ false };
        ColorRGB8 m_GuidesColorA{ 0, 0, 0 };
        ColorRGB8 m_GuidesColorB{ 190, 190, 190 };
        Length m_GuidesOffset{ 0_mm };
        Length m_GuidesThickness{ 1_pts };
        Length m_GuidesLength{ 1.5_mm };

        // Utility functions
        Size ComputePageSize(const Config& config) const;
        Size ComputeCardsSize(const Config& config) const;
        Size ComputeMargins(const Config& config) const;
        Size ComputeMaxMargins(const Config& config) const;

        float CardRatio(const Config& config) const;
        Size CardSize(const Config& config) const;
        Size CardSizeWithBleed(const Config& config) const;
        Size CardSizeWithFullBleed(const Config& config) const;
        Length CardFullBleed(const Config& config) const;
        Length CardCornerRadius(const Config& config) const;
    };
    ProjectData m_Data;

  private:
    Project(const Project&) = delete;
    Project(Project&&) = delete;
    Project& operator=(const Project&) = delete;
    Project& operator=(Project&&) = delete;
};
