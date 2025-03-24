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
    cv::Mat Page{};
    PngImageCache* ImageCache;
};

class PngImageCache
{
  public:
    const cv::Mat& GetImage(fs::path image_path, Length w, Length h, Image::Rotation rotation);

  private:
    struct ImageCacheEntry
    {
        fs::path ImagePath;
        Length Width;
        Length Height;
        Image::Rotation ImageRotation;
        cv::Mat PngImage;
    };
    std::vector<ImageCacheEntry> image_cache;
};

class PngDocument final : public PdfDocument
{
  public:
    PngDocument(PrintFn print_fn);
    virtual ~PngDocument() override;

    virtual PngPage* NextPage(Size page_size) override;

    virtual std::optional<fs::path> Write(fs::path path) override;

  private:
    std::vector<PngPage> Pages;

    std::unique_ptr<PngImageCache> ImageCache;

    PrintFn PrintFunction;
};
