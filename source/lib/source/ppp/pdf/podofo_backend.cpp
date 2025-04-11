#include <ppp/pdf/podofo_backend.hpp>

#include <dla/vector_math.h>

#include <podofo/podofo.h>

#include <ppp/pdf/util.hpp>

#include <ppp/project/project.hpp>

inline double ToPoDoFoPoints(Length l)
{
    return static_cast<double>(l / 1_pts);
}

void PoDoFoPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToPoDoFoPoints(fx) };
    const auto real_fy{ ToPoDoFoPoints(fy) };
    const auto real_tx{ ToPoDoFoPoints(tx) };
    const auto real_ty{ ToPoDoFoPoints(ty) };

    PoDoFo::PdfPainter painter;
    painter.SetPage(Page);
    painter.Save();

    painter.SetStrokeWidth(1.0);

    // First layer
    {
        painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Custom, "[1 1] 0", false, 1.0, false);

        const PoDoFo::PdfColor col{ colors[0].r, colors[0].g, colors[0].b };
        painter.SetStrokingColor(col);

        painter.DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    // Second layer with phase offset
    {
        painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Custom, "[1 1] 1", false, 1.0, false);

        const PoDoFo::PdfColor col{ colors[1].r, colors[1].g, colors[1].b };
        painter.SetStrokingColor(col);

        painter.DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    painter.Restore();
    painter.FinishPage();
}

void PoDoFoPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + 3_mm * dx };
    const auto ty{ y + 3_mm * dy };

    DrawDashedLine(colors, x, y, tx, y);
    DrawDashedLine(colors, x, y, x, ty);
}

void PoDoFoPage::DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation)
{
    const auto real_x{ ToPoDoFoPoints(x) };
    const auto real_y{ ToPoDoFoPoints(y) };
    const auto real_w{ ToPoDoFoPoints(w) };
    const auto real_h{ ToPoDoFoPoints(h) };

    auto* image{ ImageCache->GetImage(image_path, rotation) };
    const auto w_scale{ real_w / image->GetWidth() };
    const auto h_scale{ real_h / image->GetHeight() };

    PoDoFo::PdfPainter painter;
    painter.SetPage(Page);
    painter.Save();

    painter.DrawImage(real_x, real_y, image, w_scale, h_scale);

    painter.Restore();
    painter.FinishPage();
}

PoDoFoImageCache::PoDoFoImageCache(PoDoFo::PdfMemDocument* document)
    : Document{ document }
{
}

PoDoFo::PdfImage* PoDoFoImageCache::GetImage(fs::path image_path, Image::Rotation rotation)
{
    const auto it{
        std::ranges::find_if(image_cache, [&](const ImageCacheEntry& entry)
                             { return entry.ImageRotation == rotation && entry.ImagePath == image_path; })
    };
    if (it != image_cache.end())
    {
        return it->PoDoFoImage.get();
    }

    const auto use_jpg{ CFG.PdfImageFormat == ImageFormat::Jpg };
    const std::function<std::vector<std::byte>(const Image&)> encoder{
        use_jpg
            ? std::bind_back(&Image::EncodeJpg, CFG.JpgQuality)
            : std::bind_back(&Image::EncodePng, std::optional{ 0 })
    };
    const auto loader{
        use_jpg
            ? &PoDoFo::PdfImage::LoadFromJpegData
            : &PoDoFo::PdfImage::LoadFromPngData
    };

    const Image loaded_image{ Image::Read(image_path).Rotate(rotation) };
    const auto encoded_image{ encoder(loaded_image) };

    std::unique_ptr podofo_image{ std::make_unique<PoDoFo::PdfImage>(Document) };
    (podofo_image.get()->*loader)(reinterpret_cast<const unsigned char*>(encoded_image.data()), encoded_image.size());

    image_cache.push_back({
        std::move(image_path),
        rotation,
        std::move(podofo_image),
    });
    return image_cache.back().PoDoFoImage.get();
}

PoDoFoDocument::PoDoFoDocument(const Project& project, PrintFn print_fn)
    : BaseDocument{
        project.Data.PageSize == Config::BasePDFSize && LoadPdfSize(project.Data.BasePdf + ".pdf")
            ? new PoDoFo::PdfMemDocument
            : nullptr
    }
    , PrintFunction{ std::move(print_fn) }
    , ImageCache{ std::make_unique<PoDoFoImageCache>(&Document) }
{
    if (BaseDocument != nullptr)
    {
        const fs::path full_path{ "./res/base_pdfs" / fs::path{ project.Data.BasePdf + ".pdf" } };
        BaseDocument->Load(full_path.c_str());

        {
            // Some pdf files, most likely written by Cairo, have a transform at the start of the page
            // that is not wrapped with q/Q, since the assumption is that the pdf won't be edited. To
            // be able to put new stuff into the pdf we wrap the whole page in a q/Q pair
            PoDoFo::PdfPage* page{ BaseDocument->GetPage(0) };

            PoDoFo::PdfObject* q = page->GetObject()->GetOwner()->CreateObject();
            q->GetStream()->Set("q");
            PoDoFo::PdfObject* Q = page->GetObject()->GetOwner()->CreateObject();
            Q->GetStream()->Set("Q");

            auto contents{ page->GetContents() };
            contents->GetArray().insert(contents->GetArray().begin(), q->Reference());
            contents->GetArray().push_back(Q->Reference());
        }
    }
}

PoDoFoPage* PoDoFoDocument::NextPage(Size page_size)
{
    auto& new_page{ Pages.emplace_back() };
    const int new_page_idx{ static_cast<int>(Pages.size() - 1) };
    if (BaseDocument != nullptr)
    {
        new_page.Page = Document.InsertExistingPageAt(
                                    *BaseDocument,
                                    0,
                                    new_page_idx)
                            .GetPage(new_page_idx);
    }
    else
    {
        new_page.Page = Document.InsertPage(
            PoDoFo::PdfRect(
                0.0,
                0.0,
                ToPoDoFoPoints(page_size.x),
                ToPoDoFoPoints(page_size.y)),
            new_page_idx);
    }
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

std::optional<fs::path> PoDoFoDocument::Write(fs::path path)
{
    const auto pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    PPP_LOG_WITH(PrintFunction, "Saving to {}...", pdf_path_string);
    Document.Write(pdf_path.c_str());
    return pdf_path;
}
