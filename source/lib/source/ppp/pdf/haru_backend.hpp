#pragma once

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

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawSolidLine(ColorRGB32f color, Length fx, Length fy, Length tx, Length ty) override;

    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) override;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) override;

    virtual void DrawText(std::string_view text, TextBB bounding_box) override;

    virtual void Finish() override{};

  private:
    HPDF_Page Page{ nullptr };
    HaruPdfDocument* Document;
    Length CardWidth;
    Length CornerRadius;
    HaruPdfImageCache* ImageCache;
};

class HaruPdfImageCache
{
  public:
    HaruPdfImageCache(HPDF_Doc document);

    HPDF_Image GetImage(fs::path image_path, Image::Rotation rotation);

  private:
    HPDF_Doc Document;

    struct ImageCacheEntry
    {
        fs::path ImagePath;
        Image::Rotation ImageRotation;
        HPDF_Image HaruImage;
    };
    std::vector<ImageCacheEntry> image_cache;
};

class HaruPdfDocument final : public PdfDocument
{
  public:
    HaruPdfDocument(const Project& project);
    virtual ~HaruPdfDocument() override;

    virtual HaruPdfPage* NextPage() override;

    virtual fs::path Write(fs::path path) override;

    HPDF_Font GetFont();

  private:
    HPDF_Doc Document;
    const Project& TheProject;
    std::vector<HaruPdfPage> Pages;

    std::unique_ptr<HaruPdfImageCache> ImageCache;

    HPDF_Font Font{ nullptr };
};
