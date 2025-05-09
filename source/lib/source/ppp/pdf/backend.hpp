#pragma once

#include <array>
#include <memory>
#include <ranges>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

#include <ppp/pdf/util.hpp>

class Project;
class PdfDocument;

std::unique_ptr<PdfDocument> CreatePdfDocument(PdfBackend backend, const Project& project);

class PdfPage
{
  public:
    virtual ~PdfPage() = default;

    virtual void DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty) = 0;

    virtual void DrawSolidLine(ColorRGB32f color, Length fx, Length fy, Length tx, Length ty) = 0;

    enum class CrossSegment
    {
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft,
    };
    virtual void DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s) = 0;

    virtual void DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation) = 0;

    struct TextBB
    {
        Size TopLeft;
        Size BottomRight;
    };
    virtual void DrawText(std::string_view text, TextBB bounding_box) = 0;

    virtual void Finish() = 0;

  protected:
    inline static constexpr std::array CrossSegmentOffsets{
        dla::vec2{ +1.0f, -1.0f },
        dla::vec2{ -1.0f, -1.0f },
        dla::vec2{ -1.0f, +1.0f },
        dla::vec2{ +1.0f, +1.0f },
    };
};

class PdfDocument
{
  public:
    virtual ~PdfDocument() = default;

    virtual PdfPage* NextPage() = 0;

    virtual fs::path Write(fs::path path) = 0;
};
