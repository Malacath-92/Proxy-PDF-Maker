#pragma once

#include <PDFWriter/PDFPage.h>
#include <PDFWriter/PDFWriter.h>

#include <ppp/pdf/backend.hpp>

class HummusPdfImageCache;

class HummusPdfPage final : public PdfPage
{
  public:
    HummusPdfPage(PDFWriter* writer, Size page_size, HummusPdfImageCache* image_cache);

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) override;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) override;

    virtual void Finish() override;

  private:
    PDFWriter* Writer;
    PDFPage* Page;
    PageContentContext* PageContext;
    HummusPdfImageCache* ImageCache;
};

class HummusPdfImageCache
{
  public:
    HummusPdfImageCache(PDFWriter* writer);

    struct CachedImage
    {
        PDFFormXObject* Image;
        PixelSize Dimensions;
    };
    CachedImage GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    PDFWriter* Writer;

    struct ImageCacheEntry
    {
        fs::path ImagePath;
        Image::Rotation ImageRotation;
        CachedImage HummusImage;
    };
    std::vector<ImageCacheEntry> image_cache;
};

class HummusPdfDocument final : public PdfDocument
{
  public:
    HummusPdfDocument(fs::path path, PrintFn print_fn);
    virtual ~HummusPdfDocument() override = default;

    virtual HummusPdfPage* NextPage(Size page_size) override;

    virtual std::optional<fs::path> Write(fs::path path) override;

  private:
    PDFWriter Writer;
    std::vector<std::unique_ptr<HummusPdfPage>> Pages;

    std::unique_ptr<HummusPdfImageCache> ImageCache;

    PrintFn PrintFunction;
};
