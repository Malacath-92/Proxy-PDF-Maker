#pragma once

#include <ppp/pdf/haru_backend.hpp>
#include <ppp/pdf/hummus_backend.hpp>

std::unique_ptr<PdfDocument> CreatePdfDocument(PdfBackend backend, fs::path path, PrintFn print_fn)
{
    switch (backend)
    {
    case PdfBackend::LibHaru:
        return std::make_unique<HaruPdfDocument>(std::move(print_fn));
    case PdfBackend::Hummus:
        return std::make_unique<HummusPdfDocument>(std::move(path), std::move(print_fn));
    default:
        return nullptr;
    }
}
