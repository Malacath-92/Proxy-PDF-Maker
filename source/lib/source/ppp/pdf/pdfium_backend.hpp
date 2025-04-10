#pragma once

#include <fpdfview.h>

#include <ppp/pdf/backend.hpp>

class PdfiumDocument;
class PdfiumImageCache;

class PdfiumPage final : public PdfPage
{
    friend class PdfiumDocument;

  public:
    virtual ~PdfiumPage() override = default;

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) override;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) override;

    virtual void Finish() override{};

  private:
    FPDF_PAGE Page{ nullptr };
    PdfiumImageCache* ImageCache;
};

class PdfiumImageCache
{
  public:
    PdfiumImageCache(FPDF_DOCUMENT document);

    FPDF_PAGEOBJECT GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    FPDF_DOCUMENT Document;

    struct ImageCacheEntry
    {
        fs::path ImagePath;
        Image::Rotation ImageRotation;
        FPDF_PAGEOBJECT PdfiumImage;
    };
    std::vector<ImageCacheEntry> image_cache;
};

class PdfiumDocument final : public PdfDocument
{
  public:
    PdfiumDocument(PrintFn print_fn);
    virtual ~PdfiumDocument() override;

    virtual PdfiumPage* NextPage(Size page_size) override;

    virtual std::optional<fs::path> Write(fs::path path) override;

  private:
    FPDF_DOCUMENT Document;
    std::vector<PdfiumPage> Pages;

    std::unique_ptr<PdfiumImageCache> ImageCache;

    PrintFn PrintFunction;
};
