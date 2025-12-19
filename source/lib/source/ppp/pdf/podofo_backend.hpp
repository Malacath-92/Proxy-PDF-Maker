#pragma once

#include <memory>
#include <mutex>

#include <podofo/main/PdfImage.h>
#include <podofo/main/PdfMemDocument.h>
#include <podofo/main/PdfPainter.h>

#include <ppp/pdf/backend.hpp>

class PoDoFoDocument;
class PoDoFoImageCache;

class PoDoFoPage final : public PdfPage
{
    friend class PoDoFoDocument;

  public:
    virtual ~PoDoFoPage() override = default;

    virtual void SetPageName(std::string_view /*page_name*/) override{};

    virtual void DrawSolidLine(LineData data, LineStyle style) override;

    virtual void DrawDashedLine(LineData data, DashedLineStyle style) override;

    virtual void DrawImage(ImageData data) override;

    virtual void DrawText(TextData data) override;

    virtual void RotateFutureContent(Angle angle) override;

    virtual void Finish() override;

  private:
    PoDoFoPage(PoDoFo::PdfPage* page,
               PoDoFo::PdfPainter* painter,
               PoDoFoDocument* document,
               PoDoFoImageCache* image_cache);

    PoDoFo::PdfPage* m_Page{ nullptr };
    PoDoFo::PdfPainter* m_Painter{ nullptr };
    PoDoFoDocument* m_Document{ nullptr };
    PoDoFoImageCache* m_ImageCache;
};

class PoDoFoImageCache
{
  public:
    PoDoFoImageCache(PoDoFoDocument& document, const Project& project);

    PoDoFo::PdfImage* GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    mutable std::recursive_mutex m_Mutex;

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

    PoDoFo::PdfFont& GetFont();
    std::unique_ptr<PoDoFo::PdfImage> MakeImage();

    auto AquireDocumentLock();

  private:
    mutable std::mutex m_Mutex;

    const Project& m_Project;

    std::unique_ptr<PoDoFo::PdfMemDocument> m_BaseDocument;

    PoDoFo::PdfMemDocument m_Document;
    std::vector<PoDoFoPage> m_Pages;
    std::vector<std::unique_ptr<PoDoFo::PdfPainter>> m_Painters;

    std::unique_ptr<PoDoFoImageCache> m_ImageCache;
};
