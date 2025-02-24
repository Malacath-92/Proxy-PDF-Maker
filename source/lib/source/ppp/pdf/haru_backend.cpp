#include <ppp/pdf/haru_backend.hpp>

HPDF_REAL ToReal(Length l)
{
    return static_cast<HPDF_REAL>(l / 1_pts);
}

void HaruPdfPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToReal(fx) };
    const auto real_fy{ ToReal(fy) };
    const auto real_tx{ ToReal(tx) };
    const auto real_ty{ ToReal(ty) };

    HPDF_Page_SetLineWidth(Page, 1.0);

    const HPDF_REAL dash_ptn[]{ 1.0f };

    // First layer
    HPDF_Page_SetDash(Page, dash_ptn, 1, 0);
    HPDF_Page_SetRGBStroke(Page, colors[0].r, colors[0].g, colors[0].b);
    HPDF_Page_MoveTo(Page, real_fx, real_fy);
    HPDF_Page_LineTo(Page, real_tx, real_ty);
    HPDF_Page_Stroke(Page);

    // Second layer with phase offset
    HPDF_Page_SetDash(Page, dash_ptn, 1, 1);
    HPDF_Page_SetRGBStroke(Page, colors[1].r, colors[1].g, colors[1].b);
    HPDF_Page_MoveTo(Page, real_fx, real_fy);
    HPDF_Page_LineTo(Page, real_tx, real_ty);
    HPDF_Page_Stroke(Page);
}

void HaruPdfPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + 3_mm * dx };
    const auto ty{ y + 3_mm * dy };

    DrawDashedLine(colors, x, y, tx, y);
    DrawDashedLine(colors, x, y, x, ty);
}

void HaruPdfPage::DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation)
{
    const auto real_x{ ToReal(x) };
    const auto real_y{ ToReal(y) };
    const auto real_w{ ToReal(w) };
    const auto real_h{ ToReal(h) };
    HPDF_Page_DrawImage(Page, ImageCache->GetImage(image_path, rotation), real_x, real_y, real_w, real_h);
}

HaruPdfImageCache::HaruPdfImageCache(HPDF_Doc document)
    : Document{ document }
{
}

HPDF_Image HaruPdfImageCache::GetImage(fs::path image_path, Image::Rotation rotation)
{
    const auto it{
        std::ranges::find_if(image_cache, [&](const ImageCacheEntry& entry)
                             { return entry.ImageRotation == rotation && entry.ImagePath == image_path; })
    };
    if (it != image_cache.end())
    {
        return it->HaruImage;
    }

    const Image loaded_image{ Image::Read(image_path).Rotate(rotation) };
    const auto encoded_image{ loaded_image.Encode() };
    const auto libharu_image{
        HPDF_LoadPngImageFromMem(Document,
                                 reinterpret_cast<const HPDF_BYTE*>(encoded_image.data()),
                                 static_cast<HPDF_UINT>(encoded_image.size())),
    };
    image_cache.push_back({
        std::move(image_path),
        rotation,
        libharu_image,
    });
    return libharu_image;
}

HaruPdfDocument::HaruPdfDocument(PrintFn print_fn)
    : PrintFunction{ std::move(print_fn) }
{
    static constexpr auto error_handler{
        [](HPDF_STATUS error_no,
           HPDF_STATUS detail_no,
           void* user_data)
        {
            PrintFn& print_fn{ *static_cast<PrintFn*>(user_data) };
            PPP_LOG("HaruPDF ERROR: error_no={}, detail_no={}\n", error_no, detail_no);
        }
    };

    Document = HPDF_New(error_handler, &PrintFunction);
    HPDF_SetCompressionMode(Document, HPDF_COMP_ALL);

    ImageCache = std::make_unique<HaruPdfImageCache>(Document);
}
HaruPdfDocument::~HaruPdfDocument()
{
    HPDF_Free(Document);
}

HaruPdfPage* HaruPdfDocument::NextPage(Size page_size)
{
    auto& new_page{ Pages.emplace_back() };
    new_page.Page = HPDF_AddPage(Document);
    HPDF_Page_SetWidth(new_page.Page, ToReal(page_size.x));
    HPDF_Page_SetHeight(new_page.Page, ToReal(page_size.y));
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

std::optional<fs::path> HaruPdfDocument::Write(fs::path path)
{
    const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    PPP_LOG_WITH(PrintFunction, "Saving to {}...", pdf_path_string);
    HPDF_SaveToFile(Document, pdf_path_string.c_str());
    return pdf_path;
}
