#include <ppp/render_pdf.hpp>

#include <fpdfview.h>

#include <ppp/image.hpp>
#include <ppp/util/log.hpp>

#include <ppp/profile/profile.hpp>

PdfRendererDtor::~PdfRendererDtor()
{
    FPDF_DestroyLibrary();
}

PdfRendererDtor InitPdfRenderer()
{
    const FPDF_LIBRARY_CONFIG config{
        .version = 2,
        .m_pUserFontPaths = nullptr,
        .m_pIsolate = nullptr,
        .m_v8EmbedderSlot = 0,
        .m_pPlatform = nullptr,
    };
    FPDF_InitLibraryWithConfig(&config);

    return {};
}

Image RenderPdf(const fs::path& pdf_path)
{
    TRACY_AUTO_SCOPE();

    FPDF_DOCUMENT doc{ FPDF_LoadDocument(pdf_path.string().c_str(), nullptr) };
    if (!doc)
    {
        LogError("Failed to load document '{}'", pdf_path.string());
        return {};
    }

    AtScopeExit close_document{ std::bind_front(FPDF_CloseDocument, doc) };

    static constexpr auto c_PageIndex{ 0 };
    FPDF_PAGE page{ FPDF_LoadPage(doc, c_PageIndex) };
    if (!page)
    {
        LogError("Failed to load page {} of document '{}'", c_PageIndex, pdf_path.string());
        return {};
    }

    AtScopeExit close_page{ std::bind_front(FPDF_ClosePage, page) };

    static constexpr auto c_Scale{ 4.0f };
    const auto width{ static_cast<int>(FPDF_GetPageWidth(page) * c_Scale) };
    const auto height{ static_cast<int>(FPDF_GetPageHeight(page) * c_Scale) };

    FPDF_BITMAP bitmap{ FPDFBitmap_Create(width, height, true) };
    if (!bitmap)
    {
        LogError("Failed to create bitmap");
        return {};
    }

    AtScopeExit destroy_bmp{ std::bind_front(FPDFBitmap_Destroy, bitmap) };

    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0x00FFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, FPDF_ANNOT);

    auto* buffer{ FPDFBitmap_GetBuffer(bitmap) };
    const auto stride{ FPDFBitmap_GetStride(bitmap) };

    cv::Mat mat{ height, width, CV_8UC4, buffer, static_cast<size_t>(stride) };
    return Image{ mat.clone() };
}
