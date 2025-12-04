#pragma once

#include <mutex>

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

    virtual void DrawSolidLine(LineData data, LineStyle style) override;

    virtual void DrawDashedLine(LineData data, DashedLineStyle style) override;

    virtual void DrawImage(ImageData data) override;

    virtual void DrawText(TextData data) override;

    virtual void Finish() override{};

  private:
    const Project* m_Project;
    cv::Mat m_Page{};
    bool m_PerfectFit{};
    PixelSize m_CardSize{};
    PixelSize m_PageSize{};
    PngImageCache* m_ImageCache;
};

class PngImageCache
{
  public:
    PngImageCache(const Project& project);

    cv::Mat GetImage(fs::path image_path, int32_t w, int32_t h, Image::Rotation rotation);

  private:
    mutable std::mutex m_Mutex;

    const Project& m_Project;

    struct ImageCacheEntry
    {
        fs::path m_ImagePath;
        int32_t m_Width;
        int32_t m_Height;
        Image::Rotation m_ImageRotation;
        cv::Mat m_PngImage;
    };
    std::vector<ImageCacheEntry> m_Cache;
};

class PngDocument final : public PdfDocument
{
  public:
    PngDocument(const Project& project);
    virtual ~PngDocument() override;

    virtual void ReservePages(size_t pages) override;
    virtual PngPage* NextPage() override;

    virtual fs::path Write(fs::path path) override;

  private:
    mutable std::mutex m_Mutex;

    const Project& m_Project;

    PixelSize m_PrecomputedCardSize;

    Size m_PageSize;
    PixelSize m_PrecomputedPageSize;

    std::vector<PngPage> m_Pages;

    std::unique_ptr<PngImageCache> m_ImageCache;
};
