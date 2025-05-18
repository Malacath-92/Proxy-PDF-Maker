#include <ppp/pdf/haru_backend.hpp>
#include <ppp/pdf/png_backend.hpp>
#include <ppp/pdf/podofo_backend.hpp>

#include <ppp/project/project.hpp>

std::unique_ptr<PdfDocument> CreatePdfDocument(PdfBackend backend, const Project& project)
{
    switch (backend)
    {
    case PdfBackend::LibHaru:
        return std::make_unique<HaruPdfDocument>(project);
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

    const auto [dx, dy]{ c_CrossSegmentOffsets[static_cast<size_t>(data.m_Segment)].pod() };
    const auto tx{ x + data.m_Length * dx * 0.5f };
    const auto ty{ y + data.m_Length * dy * 0.5f };

    DrawSolidLine(LineData{ data.m_Pos, { tx, y } }, style);
    DrawSolidLine(LineData{ data.m_Pos, { x, ty } }, style);
}

void PdfPage::DrawDashedCross(CrossData data, DashedLineStyle style)
{
    const auto& x{ data.m_Pos.x };
    const auto& y{ data.m_Pos.y };

    const auto [dx, dy]{ c_CrossSegmentOffsets[static_cast<size_t>(data.m_Segment)].pod() };
    const auto tx{ x + data.m_Length * dx * 0.5f };
    const auto ty{ y + data.m_Length * dy * 0.5f };

    DrawDashedLine(LineData{ data.m_Pos, { tx, y } }, style);
    DrawDashedLine(LineData{ data.m_Pos, { x, ty } }, style);
}
