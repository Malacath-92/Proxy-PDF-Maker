#include <ppp/pdf/hummus_backend.hpp>

#include <PDFWriter/InputByteArrayStream.h>
#include <PDFWriter/PDFFormXObject.h>
#include <PDFWriter/PNGImageHandler.h>
#include <PDFWriter/PageContentContext.h>

inline double ToHummusPoints(Length l)
{
    return static_cast<double>(l / 1_pts);
}

HummusPdfPage::HummusPdfPage(PDFWriter* writer, Size page_size, HummusPdfImageCache* image_cache)
    : Writer{ writer }
    , ImageCache{ image_cache }
{
    Page = new PDFPage;
    Page->SetMediaBox({ 0, 0, ToHummusPoints(page_size.x), ToHummusPoints(page_size.y) });
    PageContext = Writer->StartPageContentContext(Page);
}

void HummusPdfPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    (void)colors;
    (void)fx;
    (void)fy;
    (void)tx;
    (void)ty;

    // const auto real_fx{ ToHummusPoints(fx) };
    // const auto real_fy{ ToHummusPoints(fy) };
    // const auto real_tx{ ToHummusPoints(tx) };
    // const auto real_ty{ ToHummusPoints(ty) };

    // HPDF_Page_SetLineWidth(Page, 1.0);

    // const HPDF_REAL dash_ptn[]{ 1.0f };

    //// First layer
    // HPDF_Page_SetDash(Page, dash_ptn, 1, 0);
    // HPDF_Page_SetRGBStroke(Page, colors[0].r, colors[0].g, colors[0].b);
    // HPDF_Page_MoveTo(Page, real_fx, real_fy);
    // HPDF_Page_LineTo(Page, real_tx, real_ty);
    // HPDF_Page_Stroke(Page);

    //// Second layer with phase offset
    // HPDF_Page_SetDash(Page, dash_ptn, 1, 1);
    // HPDF_Page_SetRGBStroke(Page, colors[1].r, colors[1].g, colors[1].b);
    // HPDF_Page_MoveTo(Page, real_fx, real_fy);
    // HPDF_Page_LineTo(Page, real_tx, real_ty);
    // HPDF_Page_Stroke(Page);
}

void HummusPdfPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + 3_mm * dx };
    const auto ty{ y + 3_mm * dy };

    DrawDashedLine(colors, x, y, tx, y);
    DrawDashedLine(colors, x, y, x, ty);
}

void HummusPdfPage::DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation)
{
    const auto real_x{ ToHummusPoints(x) };
    const auto real_y{ ToHummusPoints(y) };

    auto [image, size]{ ImageCache->GetImage(image_path, rotation) };

    const auto real_w{ ToHummusPoints(w) / size.x.value };
    const auto real_h{ ToHummusPoints(h) / size.y.value };

    PageContext->q();
    PageContext->cm(real_w, 0, 0, real_h, real_x, real_y);
    PageContext->Do(Page->GetResourcesDictionary().AddFormXObjectMapping(image->GetObjectID()));
    PageContext->Q();
}

void HummusPdfPage::Finish()
{
    Writer->EndPageContentContext(PageContext);
    Page = nullptr;

    Writer->WritePageAndRelease(Page);
    PageContext = nullptr;
}

HummusPdfImageCache::HummusPdfImageCache(PDFWriter* writer)
    : Writer{ writer }
{
}

HummusPdfImageCache::CachedImage HummusPdfImageCache::GetImage(fs::path image_path, Image::Rotation rotation)
{
    const auto it{
        std::ranges::find_if(image_cache, [&](const ImageCacheEntry& entry)
                             { return entry.ImageRotation == rotation && entry.ImagePath == image_path; })
    };
    if (it != image_cache.end())
    {
        return it->HummusImage;
    }

    const auto image{ Image::Read(image_path).Rotate(rotation) };
    auto encoded_image{ image.EncodePng() };
    InputByteArrayStream image_stream{
        reinterpret_cast<Byte*>(encoded_image.data()),
        static_cast<LongFilePositionType>(encoded_image.size()),
    };

    auto* hummus_image{ Writer->CreateFormXObjectFromPNGStream(&image_stream) };
    image_cache.push_back({
        std::move(image_path),
        rotation,
        CachedImage{
            hummus_image,
            image.Size(),
        },
    });
    return image_cache.back().HummusImage;
}

HummusPdfDocument::HummusPdfDocument(fs::path path, PrintFn print_fn)
    : PrintFunction{ std::move(print_fn) }
{
    {
        const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
        const auto pdf_path_string{ pdf_path.string() };
        Writer.StartPDF(pdf_path_string, ePDFVersion13);
    }

    ImageCache = std::make_unique<HummusPdfImageCache>(&Writer);
}

HummusPdfPage* HummusPdfDocument::NextPage(Size page_size)
{
    Pages.push_back(std::make_unique<HummusPdfPage>(
        &Writer,
        page_size,
        ImageCache.get()));
    return Pages.back().get();
}

std::optional<fs::path> HummusPdfDocument::Write(fs::path path)
{
    const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    PPP_LOG_WITH(PrintFunction, "Saving to {}...", pdf_path_string);

    Writer.EndPDF();
    return pdf_path;
}
