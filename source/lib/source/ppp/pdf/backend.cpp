#include <ppp/pdf/png_backend.hpp>
#include <ppp/pdf/podofo_backend.hpp>

#include <ppp/project/project.hpp>

std::unique_ptr<PdfDocument> CreatePdfDocument(PdfBackend backend, const Project& project)
{
    switch (backend)
    {
    case PdfBackend::PoDoFo:
        return std::make_unique<PoDoFoDocument>(project);
    case PdfBackend::Png:
        return std::make_unique<PngDocument>(project);
    default:
        return nullptr;
    }
}

void PdfPage::DrawSolidCross(CrossData data, LineStyle style)
{
    const auto& x{ data.m_Pos.x };
    const auto& y{ data.m_Pos.y };

    if (data.m_Segment == CrossSegment::FullCross)
    {
        const auto x1{ x - data.m_Length };
        const auto x2{ x + data.m_Length };

        const auto y1{ y - data.m_Length };
        const auto y2{ y + data.m_Length };

        DrawSolidLine(LineData{ { x1, y }, { x2, y } }, style);
        DrawSolidLine(LineData{ { x, y1 }, { x, y2 } }, style);
    }
    else
    {
        const auto [dx, dy]{ c_CrossSegmentOffsets[static_cast<size_t>(data.m_Segment)].pod() };
        const auto tx{ x + data.m_Length * dx };
        const auto ty{ y + data.m_Length * dy };

        DrawSolidLine(LineData{ data.m_Pos, { tx, y } }, style);
        DrawSolidLine(LineData{ data.m_Pos, { x, ty } }, style);
    }
}

void PdfPage::DrawDashedCross(CrossData data, DashedLineStyle style)
{
    const auto& x{ data.m_Pos.x };
    const auto& y{ data.m_Pos.y };

    if (data.m_Segment == CrossSegment::FullCross)
    {
        const auto x1{ x - data.m_Length };
        const auto x2{ x + data.m_Length };

        const auto y1{ y - data.m_Length };
        const auto y2{ y + data.m_Length };

        DrawDashedLine(LineData{ { x1, y }, { x2, y } }, style);
        DrawDashedLine(LineData{ { x, y1 }, { x, y2 } }, style);
    }
    else
    {
        const auto [dx, dy]{ c_CrossSegmentOffsets[static_cast<size_t>(data.m_Segment)].pod() };
        const auto tx{ x + data.m_Length * dx };
        const auto ty{ y + data.m_Length * dy };

        DrawDashedLine(LineData{ data.m_Pos, { tx, y } }, style);
        DrawDashedLine(LineData{ data.m_Pos, { x, ty } }, style);
    }
}
