#include <ppp/pdf/podofo_backend.hpp>

#include <dla/vector_math.h>

#include <podofo/podofo.h>

#include <ppp/pdf/util.hpp>

#include <ppp/util/log.hpp>

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
    const auto line_width{ CardWidth / 2.5_in };
    const auto dash_size{ ToPoDoFoPoints(CornerRadius) / 10.0f };

    PoDoFo::PdfPainter painter;
    painter.SetPage(Page);
    painter.Save();

    painter.SetStrokeWidth(line_width);

    // First layer
    {
        const std::string dash{ fmt::format("[{0:.2f} {0:.2f}] 0", dash_size) };
        painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Custom, dash.c_str(), false, 1.0, false);

        const PoDoFo::PdfColor col{ colors[0].r, colors[0].g, colors[0].b };
        painter.SetStrokingColor(col);

        painter.DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    // Second layer with phase offset
    {
        const std::string dash{ fmt::format("[{0:.2f} {0:.2f}] {0:.2f}", dash_size) };
        painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Custom, dash.c_str(), false, 1.0, false);

        const PoDoFo::PdfColor col{ colors[1].r, colors[1].g, colors[1].b };
        painter.SetStrokingColor(col);

        painter.DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    painter.Restore();
    painter.FinishPage();
}

void PoDoFoPage::DrawSolidLine(ColorRGB32f color, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToPoDoFoPoints(fx) };
    const auto real_fy{ ToPoDoFoPoints(fy) };
    const auto real_tx{ ToPoDoFoPoints(tx) };
    const auto real_ty{ ToPoDoFoPoints(ty) };
    const auto line_width{ CardWidth / 2.5_in };
    const PoDoFo::PdfColor col{ color.r, color.g, color.b };

    PoDoFo::PdfPainter painter;
    painter.SetPage(Page);
    painter.Save();

    painter.SetStrokeWidth(line_width);
    painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Solid, nullptr, false, 1.0, false);
    painter.SetStrokingColor(col);
    painter.DrawLine(real_fx, real_fy, real_tx, real_ty);

    painter.Restore();
    painter.FinishPage();
}

void PoDoFoPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + CornerRadius * dx * 0.5f };
    const auto ty{ y + CornerRadius * dy * 0.5f };

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

void PoDoFoPage::DrawText(std::string_view text, TextBB bounding_box)
{
    const PoDoFo::PdfRect rect{
        ToPoDoFoPoints(bounding_box.TopLeft.x),
        ToPoDoFoPoints(bounding_box.BottomRight.y),
        ToPoDoFoPoints(bounding_box.BottomRight.x - bounding_box.TopLeft.x),
        ToPoDoFoPoints(bounding_box.TopLeft.y - bounding_box.BottomRight.y),
    };

    PoDoFo::PdfPainter painter;
    painter.SetPage(Page);
    painter.Save();

    painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Solid, nullptr, false, 1.0, false);
    painter.SetFont(Document->GetFont());
    painter.DrawMultiLineText(rect,
                              PoDoFo::PdfString{ text.data(), static_cast<PoDoFo::pdf_long>(text.size()) },
                              PoDoFo::ePdfAlignment_Center,
                              PoDoFo::ePdfVerticalAlignment_Center);

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
            ? [](const Image& image)
            { return image.EncodeJpg(CFG.JpgQuality); }
            : [](const Image& image)
            { return image.EncodePng(std::optional{ 0 }); }
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

PoDoFoDocument::PoDoFoDocument(const Project& project)
    : BaseDocument{
        project.Data.PageSize == Config::BasePDFSize && LoadPdfSize(project.Data.BasePdf + ".pdf")
            ? new PoDoFo::PdfMemDocument
            : nullptr
    }
    , TheProject{ project }
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

PoDoFoPage* PoDoFoDocument::NextPage()
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
        const auto page_size{ TheProject.ComputePageSize() };
        new_page.Page = Document.InsertPage(
            PoDoFo::PdfRect(
                0.0,
                0.0,
                ToPoDoFoPoints(page_size.x),
                ToPoDoFoPoints(page_size.y)),
            new_page_idx);
    }
    new_page.Document = this;
    new_page.CardWidth = TheProject.CardSize().x;
    new_page.CornerRadius = TheProject.CardCornerRadius();
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

fs::path PoDoFoDocument::Write(fs::path path)
{
    try
    {
        const auto pdf_path{ fs::path{ path }.replace_extension(".pdf") };
        const auto pdf_path_string{ pdf_path.string() };
        LogInfo("Saving to {}...", pdf_path_string);
        Document.Write(pdf_path.c_str());
        return pdf_path;
    }
    catch (const PoDoFo::PdfError& e)
    {
        // Rethrow as a std::exception so the agnostic code can catch it
        throw std::logic_error{ e.what() };
    }
}

PoDoFo::PdfFont* PoDoFoDocument::GetFont()
{
    if (Font == nullptr)
    {
        Font = Document.CreateFont("arial");
    }
    return Font;
}
