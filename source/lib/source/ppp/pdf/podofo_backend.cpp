#include <ppp/pdf/podofo_backend.hpp>

#include <dla/vector_math.h>

#include <podofo/podofo.h>

#include <ppp/pdf/util.hpp>

#include <ppp/project/project.hpp>

inline double ToPoDoFoPoints(Length l)
{
    return static_cast<double>(l / 1_pts);
}

// Some pdf files, most likely written by Cairo, have a transform at the start of the page
// that is not wrapped with q/Q, since the assumption is that the pdf won't be edited. To
// be able to put new stuff into the pdf we have to extract the transform, then invert it
// and then apply the inverse of said transform on each operation
std::optional<PdfTransform> GetUnprotectedPageTransform(PoDoFo::PdfPage* page)
{
    PoDoFo::PdfContentsTokenizer tokenizer{ page };

    PoDoFo::EPdfContentsType type{};
    const char* token{};
    PoDoFo::PdfVariant param{};

    std::vector<PoDoFo::PdfVariant> params;

    while (tokenizer.ReadNext(type, token, param))
    {
        if (type == PoDoFo::ePdfContentsType_Keyword)
        {
            using namespace std::string_view_literals;
            if (token == "q"sv)
            {
                // Trust the written pdf that this is paired with enough Q to give us a blank slate
                break;
            }
            else if (token == "cm"sv)
            {
                // cm sets transformation, must be 6 values
                if (params.size() == 6)
                {
                    const double tf_a = params[0].GetReal();
                    const double tf_b = params[1].GetReal();
                    const double tf_c = params[2].GetReal();
                    const double tf_d = params[3].GetReal();
                    const double tf_e = params[4].GetReal();
                    const double tf_f = params[5].GetReal();
                    return PdfTransform{ tf_a, tf_b, tf_c, tf_d, tf_e, tf_f };
                }
            }
            params.clear();
        }
        else if (type == PoDoFo::ePdfContentsType_Variant)
        {
            params.push_back(param);
        }
    }

    return std::nullopt;
}

PdfTransform InvertTransform(PdfTransform transform)
{
    const auto& s{ transform.ScaleRotation };
    const auto& t{ transform.Translation };
    const double determinant{ s[0][0] * s[1][1] - s[1][0] * s[0][1] };
    if (std::abs(determinant) < 1e-10)
    {
        return PdfTransform{
            1, 0, 0, 1, -t[0], -t[1]
        };
    }
    else
    {
        const double inverse_det{ 1.0 / determinant };
        const double id{ inverse_det };
        return PdfTransform{
            id * s[1][1],
            -id * s[1][0],
            -id * s[0][1],
            id * s[0][0],
            id * (t[1] * s[0][1] - s[1][1] * t[0]),
            id * (-t[1] * s[0][0] + s[1][0] * t[0]),
        };
    }
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

    if (BaseTransform.has_value())
    {
        const auto& s{ BaseTransform->ScaleRotation };
        const auto& t{ BaseTransform->Translation };
        painter.SetTransformationMatrix(s[0][0], s[0][1], s[1][0], s[1][1], t[0], t[1]);
    }

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

    if (BaseTransform.has_value())
    {
        const auto& s{ BaseTransform->ScaleRotation };
        const auto& t{ BaseTransform->Translation };
        painter.SetTransformationMatrix(s[0][0], s[0][1], s[1][0], s[1][1], t[0], t[1]);
    }

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

        if (const auto transform{ GetUnprotectedPageTransform(BaseDocument->GetPage(0)) })
        {
            const auto inverse_transform{ InvertTransform(transform.value()) };
            BaseTransform = inverse_transform;
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
        new_page.BaseTransform = BaseTransform;
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
