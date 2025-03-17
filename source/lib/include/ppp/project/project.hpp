#pragma once

#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

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
using CardMap = std::unordered_map<fs::path, CardInfo>;

struct ImagePreview
{
    Image UncroppedImage;
    Image CroppedImage;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

class Project
{
  public:
    Project() = default;

    void Load(const fs::path& json_path, const cv::Mat* color_cube, PrintFn print_fn);
    void Dump(const fs::path& json_path, PrintFn print_fn) const;

    void Init(const cv::Mat* color_cube, PrintFn print_fn);
    void InitProperties(PrintFn print_fn);
    void InitImages(const cv::Mat* color_cube, PrintFn print_fn);

    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    bool HasPreview(const fs::path& image_name) const;
    const Image& GetCroppedPreview(const fs::path& image_name) const;
    const Image& GetUncroppedPreview(const fs::path& image_name) const;
    const Image& GetCroppedBacksidePreview(const fs::path& image_name) const;
    const Image& GetUncroppedBacksidePreview(const fs::path& image_name) const;

    void SetPreview(const fs::path& image_name, ImagePreview preview);

    ImgDict GetPreviews() const;
    void SetPreviews(ImgDict previews);

    const fs::path& GetBacksideImage(const fs::path& image_name) const;

    // Project options
    fs::path ImageDir{ "images" };
    fs::path CropDir{ "images/crop" };
    fs::path ImageCache{ "proj.cache" };

    // List of all cards
    CardMap Cards{};
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
    std::string Orientation{ "Portrait" };
    fs::path FileName{ "_printme" };
    bool EnableGuides{ true };
    bool ExtendedGuides{ false };
    ColorRGB8 GuidesColorA{ 0, 0, 0 };
    ColorRGB8 GuidesColorB{ 190, 190, 190 };

  private:
    struct PreviewData
    {
        // Previews are private to guarantee thread-safe access
        std::shared_mutex PreviewsMutex;
        ImgDict Previews{};
    };
    std::unique_ptr<PreviewData> Previews{ nullptr };

    Project(const Project&) = delete;
    Project(Project&&) = default;
    Project& operator=(const Project&) = delete;
    Project& operator=(Project&&) = default;
};
