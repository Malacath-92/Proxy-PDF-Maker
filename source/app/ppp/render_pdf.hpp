#pragma once

#include <ppp/util.hpp>

struct [[nodiscard]] PdfRendererDtor
{
    ~PdfRendererDtor();
};

PdfRendererDtor InitPdfRenderer();
class Image RenderPdf(const fs::path& pdf_path);
