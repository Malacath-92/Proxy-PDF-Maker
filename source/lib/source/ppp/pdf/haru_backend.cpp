#include <ppp/pdf/haru_backend.hpp>

inline HPDF_REAL ToHaruReal(Length l)
{
    return static_cast<HPDF_REAL>(l / 1_pts);
}

void HaruPdfPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToHaruReal(fx) };
    const auto real_fy{ ToHaruReal(fy) };
    const auto real_tx{ ToHaruReal(tx) };
    const auto real_ty{ ToHaruReal(ty) };

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
    const auto tx{ x + CFG.CardCornerRadius.Dimension * dx };
    const auto ty{ y + CFG.CardCornerRadius.Dimension * dy };

    DrawDashedLine(colors, x, y, tx, y);
    DrawDashedLine(colors, x, y, x, ty);
}

void HaruPdfPage::DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation)
{
    const auto real_x{ ToHaruReal(x) };
    const auto real_y{ ToHaruReal(y) };
    const auto real_w{ ToHaruReal(w) };
    const auto real_h{ ToHaruReal(h) };
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

    const auto use_jpg{ CFG.PdfImageFormat == ImageFormat::Jpg };
    const std::function<std::vector<std::byte>(const Image&)> encoder{
        use_jpg
            ? std::bind_back(&Image::EncodeJpg, CFG.JpgQuality)
            : std::bind_back(&Image::EncodePng, std::optional{ 0 })
    };
    const auto loader{
        use_jpg
            ? &HPDF_LoadJpegImageFromMem
            : &HPDF_LoadPngImageFromMem
    };

    const Image loaded_image{ Image::Read(image_path).Rotate(rotation) };
    const auto encoded_image{ encoder(loaded_image) };
    const auto libharu_image{
        loader(Document,
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
    HPDF_Page_SetWidth(new_page.Page, ToHaruReal(page_size.x));
    HPDF_Page_SetHeight(new_page.Page, ToHaruReal(page_size.y));
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

fs::path HaruPdfDocument::Write(fs::path path)
{
    const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    PPP_LOG_WITH(PrintFunction, "Saving to {}...", pdf_path_string);
    HPDF_SaveToFile(Document, pdf_path_string.c_str());
    return pdf_path;
}
