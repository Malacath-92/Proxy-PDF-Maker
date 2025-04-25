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
