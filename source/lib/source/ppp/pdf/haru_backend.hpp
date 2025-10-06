#pragma once

#include <mutex>

#include <hpdf.h>

#include <ppp/pdf/backend.hpp>

class Project;

class HaruPdfDocument;
class HaruPdfImageCache;

class HaruPdfPage final : public PdfPage
{
    friend class HaruPdfDocument;

  public:
    virtual ~HaruPdfPage() override = default;

    virtual void DrawSolidLine(LineData data, LineStyle style) override;

    virtual void DrawDashedLine(LineData data, DashedLineStyle style) override;

    virtual void DrawImage(ImageData data) override;

    virtual void DrawText(std::string_view text, TextBoundingBox bounding_box) override;

    virtual void Finish() override{};

  private:
    HPDF_Page m_Page{ nullptr };
    HaruPdfDocument* m_Document;
    HaruPdfImageCache* m_ImageCache;
};

class HaruPdfImageCache
{
  public:
    HaruPdfImageCache(HaruPdfDocument& document, const Project& project);

    HPDF_Image GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    mutable std::mutex m_Mutex;

    HaruPdfDocument& m_Document;
    const Project& m_Project;

    struct ImageCacheEntry
    {
        fs::path m_ImagePath;
        Image::Rotation m_ImageRotation;
        HPDF_Image m_HaruImage;
    };
    std::vector<ImageCacheEntry> m_Cache;
};

class HaruPdfDocument final : public PdfDocument
{
  public:
    HaruPdfDocument(const Project& project);
    virtual ~HaruPdfDocument() override;

    virtual void ReservePages(size_t pages) override;
    virtual HaruPdfPage* NextPage() override;

    virtual fs::path Write(fs::path path) override;

    HPDF_Font GetFont();
    HPDF_Image MakeImage(bool use_png, std::span<const std::byte> encoded_image);

  private:
    mutable std::mutex m_Mutex;

    const Project& m_Project;

    HPDF_Doc m_Document;
    std::vector<HaruPdfPage> m_Pages;

    std::unique_ptr<HaruPdfImageCache> m_ImageCache;

    HPDF_Font m_Font{ nullptr };
};
