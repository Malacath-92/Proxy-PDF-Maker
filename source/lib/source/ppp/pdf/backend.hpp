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

    struct LineData
    {
        Position m_From;
        Position m_To;
    };

    struct LineStyle
    {
        Length m_Thickness{ 0.5_mm };
        ColorRGB32f m_Color;
    };

    struct DashedLineStyle : LineStyle
    {
        ColorRGB32f m_SecondColor;
        Length m_TargetDashSize{ m_Thickness };
    };

    enum class CrossSegment
    {
        TopLeft,
        TopRight,
        BottomRight,
        BottomLeft,
        FullCross,
    };

    struct CrossData
    {
        Position m_Pos;
        Length m_Length;
        CrossSegment m_Segment;
    };

    struct ImageData
    {
        const fs::path& m_Path;
        Position m_Pos;
        Size m_Size;
        Image::Rotation m_Rotation;
    };

    struct TextBoundingBox
    {
        Size m_TopLeft;
        Size m_BottomRight;
    };

    struct TextData
    {
        std::string_view m_Text;
        TextBoundingBox m_BoundingBox;
        std::optional<ColorRGB32f> m_Backdrop{ std::nullopt };
    };

    virtual void DrawSolidLine(LineData data, LineStyle style) = 0;

    virtual void DrawDashedLine(LineData data, DashedLineStyle style) = 0;

    virtual void DrawSolidCross(CrossData data, LineStyle style);

    virtual void DrawDashedCross(CrossData data, DashedLineStyle style);

    virtual void DrawImage(ImageData data) = 0;

    virtual void DrawText(TextData data) = 0;

    virtual void Finish() = 0;

  protected:
    inline static constexpr std::array c_CrossSegmentOffsets{
        dla::vec2{ +1.0f, -1.0f },
        dla::vec2{ -1.0f, -1.0f },
        dla::vec2{ -1.0f, +1.0f },
        dla::vec2{ +1.0f, +1.0f },
    };

    static Length ComputeFinalDashSize(Length line_length, Length dash_size);
};

class PdfDocument
{
  public:
    virtual ~PdfDocument() = default;

    virtual void ReservePages(size_t pages) = 0;
    virtual PdfPage* NextPage() = 0;

    virtual fs::path Write(fs::path path) = 0;
};
