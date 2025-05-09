#pragma once

#include <memory>

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

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawSolidLine(ColorRGB32f color, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) override;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) override;

    virtual void DrawText(std::string_view text, TextBB bounding_box) override;

    virtual void Finish() override{};

  private:
    PoDoFo::PdfPage* Page{ nullptr };
    PoDoFoDocument* Document{ nullptr };
    Length CardWidth;
    Length CornerRadius;
    PoDoFoImageCache* ImageCache;
};

class PoDoFoImageCache
{
  public:
    PoDoFoImageCache(PoDoFo::PdfMemDocument* document);

    PoDoFo::PdfImage* GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    PoDoFo::PdfMemDocument* Document;

    struct ImageCacheEntry
    {
        fs::path ImagePath;
        Image::Rotation ImageRotation;
        std::unique_ptr<PoDoFo::PdfImage> PoDoFoImage;
    };
    std::vector<ImageCacheEntry> image_cache;
};

class PoDoFoDocument final : public PdfDocument
{
  public:
    PoDoFoDocument(const Project& project);
    virtual ~PoDoFoDocument() override = default;

    virtual PoDoFoPage* NextPage() override;

    virtual fs::path Write(fs::path path) override;

    PoDoFo::PdfFont* GetFont();

  private:
    std::unique_ptr<PoDoFo::PdfMemDocument> BaseDocument;

    const Project& TheProject;

    PoDoFo::PdfMemDocument Document;
    std::vector<PoDoFoPage> Pages;

    std::unique_ptr<PoDoFoImageCache> ImageCache;

    PoDoFo::PdfFont* Font{ nullptr };
};
