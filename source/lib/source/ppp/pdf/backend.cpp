#include <ppp/pdf/haru_backend.hpp>
#include <ppp/pdf/hummus_backend.hpp>
#include <ppp/pdf/png_backend.hpp>

#include <ppp/project/project.hpp>

std::unique_ptr<PdfDocument> CreatePdfDocument(PdfBackend backend, const Project& project, PrintFn print_fn)
{
    switch (backend)
    {
    case PdfBackend::LibHaru:
        return std::make_unique<HaruPdfDocument>(std::move(print_fn));
    case PdfBackend::Hummus:
        return std::make_unique<HummusPdfDocument>(project.Data.FileName, std::move(print_fn));
    case PdfBackend::Png:
        return std::make_unique<PngDocument>(project, std::move(print_fn));
    default:
        return nullptr;
    }
}
