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
    uint32_t Num{ 1 };
    fs::path Backside{};
    bool BacksideShortEdge{ false };
    bool Oversized{ false };
};
using CardMap = std::map<fs::path, CardInfo>;

struct ImagePreview
{
    Image UncroppedImage;
    Image CroppedImage;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

class Project : public QObject
{
    Q_OBJECT;

  public:
    Project() = default;
    ~Project();

    void Load(const fs::path& json_path, PrintFn print_fn);
    void Dump(const fs::path& json_path, PrintFn print_fn) const;

    void Init(PrintFn print_fn);
    void InitProperties(PrintFn print_fn);

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    bool HasPreview(const fs::path& image_name) const;
    const Image& GetCroppedPreview(const fs::path& image_name) const;
    const Image& GetUncroppedPreview(const fs::path& image_name) const;
    const Image& GetCroppedBacksidePreview(const fs::path& image_name) const;
    const Image& GetUncroppedBacksidePreview(const fs::path& image_name) const;

    const fs::path& GetBacksideImage(const fs::path& image_name) const;

  public slots:
    void SetPreview(const fs::path& image_name, ImagePreview preview);

    void CropperDone();

  signals:
    void PreviewUpdated(const fs::path& image_name, const ImagePreview& preview);

  public:
    struct ProjectData
    {
        // Project options
        fs::path ImageDir{ "images" };
        fs::path CropDir{ "images/crop" };
        fs::path ImageCache{ "images/crop/preview.cache" };

        // List of all cards
        CardMap Cards{};
        ImgDict Previews{};
        ImagePreview FallbackPreview{};

        // Bleed edge options
        Length BleedEdge{ 0_mm };
        float CornerWeight{ 1.0f };

        // Backside options
        bool BacksideEnabled{ false };
        bool BacksideEnableGuides{ false };
        fs::path BacksideDefault{ "__back.png" };
        Length BacksideOffset{ 0_mm };

        // Oversized options
        bool OversizedEnabled{ false };

        // PDF generation options
        std::string PageSize{ CFG.DefaultPageSize };
        dla::uvec2 CardLayout{ 3, 3 };
        std::string Orientation{ "Portrait" };
        fs::path FileName{ "_printme" };
        bool EnableGuides{ true };
        bool ExtendedGuides{ false };
        ColorRGB8 GuidesColorA{ 0, 0, 0 };
        ColorRGB8 GuidesColorB{ 190, 190, 190 };
    };
    ProjectData Data;

  private:
    Project(const Project&) = default;
    Project(Project&&) = default;
    Project& operator=(const Project&) = default;
    Project& operator=(Project&&) = default;
};
