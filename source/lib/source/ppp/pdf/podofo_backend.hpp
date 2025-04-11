#pragma once

#include <memory>

#include <podofo/doc/PdfImage.h>
#include <podofo/doc/PdfMemDocument.h>

#include <ppp/pdf/backend.hpp>

class PoDoFoDocument;
class PoDoFoImageCache;

struct PdfTransform
{
    double ScaleRotation[2][2];
    double Translation[2];
};

class PoDoFoPage final : public PdfPage
{
    friend class PoDoFoDocument;

  public:
    virtual ~PoDoFoPage() override = default;

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) override;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) override;

    virtual void Finish() override {};

  private:
    PoDoFo::PdfPage* Page{ nullptr };
    std::optional<PdfTransform> BaseTransform;
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
    PoDoFoDocument(const Project& project, PrintFn print_fn);
    virtual ~PoDoFoDocument() override = default;

    virtual PoDoFoPage* NextPage(Size page_size) override;

    virtual std::optional<fs::path> Write(fs::path path) override;

  private:
    std::unique_ptr<PoDoFo::PdfMemDocument> BaseDocument;
    std::optional<PdfTransform> BaseTransform;

    PoDoFo::PdfMemDocument Document;
    std::vector<PoDoFoPage> Pages;

    std::unique_ptr<PoDoFoImageCache> ImageCache;

    PrintFn PrintFunction;
};
