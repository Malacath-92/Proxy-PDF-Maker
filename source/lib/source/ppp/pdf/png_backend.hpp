#pragma once

#include <opencv2/opencv.hpp>

#include <ppp/image.hpp>

#include <ppp/pdf/backend.hpp>

class PngDocument;
class PngImageCache;

class PngPage final : public PdfPage
{
    friend class PngDocument;

  public:
    virtual ~PngPage() override = default;

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) override;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) override;

    virtual void Finish() override{};

  private:
    const Project* TheProject;
    cv::Mat Page{};
    bool PerfectFit{};
    PixelSize CardSize{};
    PixelSize PageSize{};
    Length CornerRadius;
    PngImageCache* ImageCache;
};

class PngImageCache
{
  public:
    const cv::Mat& GetImage(fs::path image_path, int32_t w, int32_t h, Image::Rotation rotation);

  private:
    struct ImageCacheEntry
    {
        fs::path ImagePath;
        int32_t Width;
        int32_t Height;
        Image::Rotation ImageRotation;
        cv::Mat PngImage;
    };
    std::vector<ImageCacheEntry> image_cache;
};

class PngDocument final : public PdfDocument
{
  public:
    PngDocument(const Project& project, PrintFn print_fn);
    virtual ~PngDocument() override;

    virtual PngPage* NextPage() override;

    virtual fs::path Write(fs::path path) override;

  private:
    const Project& TheProject;

    PixelSize PrecomputedCardSize;
    PixelSize PrecomputedPageSize;

    Size PageSize;

    std::vector<PngPage> Pages;

    std::unique_ptr<PngImageCache> ImageCache;

    PrintFn PrintFunction;
};
