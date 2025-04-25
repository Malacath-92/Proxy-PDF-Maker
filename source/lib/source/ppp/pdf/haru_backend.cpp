#include <ppp/pdf/haru_backend.hpp>

#include <QStandardPaths>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

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
    const auto line_width{ CardWidth / 2.5_in };
    const auto dash_size{ ToHaruReal(CornerRadius) / 10.0f };

    HPDF_Page_SetLineWidth(Page, line_width);

    const HPDF_REAL dash_ptn[]{ dash_size };

    // First layer
    HPDF_Page_SetDash(Page, dash_ptn, 1, 0.0f);
    HPDF_Page_SetRGBStroke(Page, colors[0].r, colors[0].g, colors[0].b);
    HPDF_Page_MoveTo(Page, real_fx, real_fy);
    HPDF_Page_LineTo(Page, real_tx, real_ty);
    HPDF_Page_Stroke(Page);

    // Second layer with phase offset
    HPDF_Page_SetDash(Page, dash_ptn, 1, dash_size);
    HPDF_Page_SetRGBStroke(Page, colors[1].r, colors[1].g, colors[1].b);
    HPDF_Page_MoveTo(Page, real_fx, real_fy);
    HPDF_Page_LineTo(Page, real_tx, real_ty);
    HPDF_Page_Stroke(Page);
}

void HaruPdfPage::DrawSolidLine(ColorRGB32f color, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToHaruReal(fx) };
    const auto real_fy{ ToHaruReal(fy) };
    const auto real_tx{ ToHaruReal(tx) };
    const auto real_ty{ ToHaruReal(ty) };
    const auto line_width{ CardWidth / 2.5_in };

    HPDF_Page_SetLineWidth(Page, line_width);
    HPDF_Page_SetRGBStroke(Page, color.r, color.g, color.b);
    HPDF_Page_MoveTo(Page, real_fx, real_fy);
    HPDF_Page_LineTo(Page, real_tx, real_ty);
    HPDF_Page_Stroke(Page);
}

void HaruPdfPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + CornerRadius * dx * 0.5f };
    const auto ty{ y + CornerRadius * dy * 0.5f };

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

void HaruPdfPage::DrawText(std::string_view text, TextBB bounding_box)
{
    const auto left{ ToHaruReal(bounding_box.TopLeft.x) };
    const auto top{ ToHaruReal(bounding_box.TopLeft.y) };
    const auto right{ ToHaruReal(bounding_box.BottomRight.x) };
    const auto bottom{ ToHaruReal(bounding_box.BottomRight.y) };

    HPDF_Page_SetFontAndSize(Page, Document->GetFont(), 12);

    char text_cpy[1024]{};
    text.copy(text_cpy, std::min(text.size(), sizeof(text_cpy) - 1));
    HPDF_Page_BeginText(Page);
    HPDF_Page_TextRect(Page, left, top, right, bottom, text_cpy, HPDF_TALIGN_CENTER, nullptr);
    HPDF_Page_EndText(Page);
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
            ? [](const Image& image)
            { return image.EncodeJpg(CFG.JpgQuality); }
            : [](const Image& image)
            { return image.EncodePng(std::optional{ 0 }); }
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

HaruPdfDocument::HaruPdfDocument(const Project& project)
    : TheProject{ project }
{
    static constexpr auto error_handler{
        [](HPDF_STATUS error_no,
           HPDF_STATUS detail_no,
           [[maybe_unused]] void* user_data)
        {
            LogError("HaruPDF ERROR: error_no={}, detail_no={}", error_no, detail_no);
        }
    };

    Document = HPDF_New(error_handler, nullptr);
    HPDF_SetCompressionMode(Document, HPDF_COMP_ALL);

    ImageCache = std::make_unique<HaruPdfImageCache>(Document);
}
HaruPdfDocument::~HaruPdfDocument()
{
    HPDF_Free(Document);
}

HaruPdfPage* HaruPdfDocument::NextPage()
{
    const auto page_size{ TheProject.ComputePageSize() };
    auto& new_page{ Pages.emplace_back() };
    new_page.Page = HPDF_AddPage(Document);
    new_page.Document = this;
    new_page.CardWidth = TheProject.CardSize().x;
    new_page.CornerRadius = TheProject.CardCornerRadius();
    HPDF_Page_SetWidth(new_page.Page, ToHaruReal(page_size.x));
    HPDF_Page_SetHeight(new_page.Page, ToHaruReal(page_size.y));
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

fs::path HaruPdfDocument::Write(fs::path path)
{
    const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    LogInfo("Saving to {}...", pdf_path_string);
    HPDF_SaveToFile(Document, pdf_path_string.c_str());
    return pdf_path;
}
HPDF_Font HaruPdfDocument::GetFont()
{
    if (Font == nullptr)
    {
        const auto arial_path{ QStandardPaths::locate(QStandardPaths::FontsLocation, "arial.ttf") };
        const auto arial_font_name{ HPDF_LoadTTFontFromFile(Document, arial_path.toStdString().c_str(), true) };
        Font = HPDF_GetFont(Document, arial_font_name, HPDF_ENCODING_FONT_SPECIFIC);
    }
    return Font;
}
