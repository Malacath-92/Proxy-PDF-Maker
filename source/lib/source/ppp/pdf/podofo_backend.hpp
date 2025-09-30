#pragma once

#include <memory>
#include <mutex>

#include <podofo/doc/PdfImage.h>
#include <podofo/doc/PdfMemDocument.h>

#include <ppp/pdf/backend.hpp>

class PoDoFoDocument;
class PoDoFoImageCache;

class PoDoFoPage final : public PdfPage
{
    friend class PoDoFoDocument;

  public:
    virtual ~PoDoFoPage() override = default;

    virtual void DrawSolidLine(LineData data, LineStyle style) override;

    virtual void DrawDashedLine(LineData data, DashedLineStyle style) override;

    virtual void DrawImage(ImageData data) override;

    virtual void DrawText(std::string_view text, TextBoundingBox bounding_box) override;

    virtual void Finish() override {};

  private:
    PoDoFo::PdfPage* m_Page{ nullptr };
    PoDoFoDocument* m_Document{ nullptr };
    PoDoFoImageCache* m_ImageCache;
};

class PoDoFoImageCache
{
  public:
    PoDoFoImageCache(PoDoFoDocument& document, const Project& project);

    PoDoFo::PdfImage* GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    mutable std::mutex m_Mutex;

    PoDoFoDocument& m_Document;
    const Project& m_Project;

    struct ImageCacheEntry
    {
        fs::path m_ImagePath;
        Image::Rotation m_ImageRotation;
        std::unique_ptr<PoDoFo::PdfImage> m_PoDoFoImage;
    };
    std::vector<ImageCacheEntry> m_Cache;
};

class PoDoFoDocument final : public PdfDocument
{
  public:
    PoDoFoDocument(const Project& project);
    virtual ~PoDoFoDocument() override = default;

    virtual void ReservePages(size_t pages) override;
    virtual PoDoFoPage* NextPage() override;

    virtual fs::path Write(fs::path path) override;

    PoDoFo::PdfFont* GetFont();
    std::unique_ptr<PoDoFo::PdfImage> MakeImage();

  private:
    mutable std::mutex m_Mutex;

    const Project& m_Project;

    std::unique_ptr<PoDoFo::PdfMemDocument> m_BaseDocument;

    PoDoFo::PdfMemDocument m_Document;
    std::vector<PoDoFoPage> m_Pages;

    std::unique_ptr<PoDoFoImageCache> m_ImageCache;

    PoDoFo::PdfFont* m_Font{ nullptr };
};
