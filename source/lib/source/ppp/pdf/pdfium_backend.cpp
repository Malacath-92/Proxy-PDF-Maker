#include <ppp/pdf/pdfium_backend.hpp>

#include <dla/vector_math.h>

#include <fpdf_edit.h>
#include <fpdf_save.h>

inline float ToPdfiumPoints(Length l)
{
    return static_cast<float>(l / 1_pts);
}

void PdfiumPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToPdfiumPoints(fx) };
    const auto real_fy{ ToPdfiumPoints(fy) };
    const auto real_tx{ ToPdfiumPoints(tx) };
    const auto real_ty{ ToPdfiumPoints(ty) };

    // Should just use FPDFPageObj_SetDashArray but it does not seem to work
    const auto draw_dashed_line{
        [&](auto col, float dash, float phase)
        {
            const dla::tvec2 from{ real_fx, real_fy };
            const dla::tvec2 to{ real_tx, real_ty };
            const dla::tvec2 diff{ to - from };
            const float dist{ diff.length() };
            const dla::tvec2 delta{ diff / dist };
            for (auto d = phase; d < dist; d += dash)
            {
                auto f{ from + d * delta };
                auto t{ from + (d + dash / 2.0f) * delta };

                FPDF_PAGEOBJECT path{ FPDFPageObj_CreateNewPath(f.x, f.y) };
                FPDFPath_SetDrawMode(path, FPDF_FILLMODE_NONE, 1);
                FPDFPageObj_SetStrokeWidth(path, 1.0f);
                FPDFPath_LineTo(path, t.x, t.y);
                FPDFPageObj_SetStrokeColor(path, col.r, col.g, col.b, 255);
                FPDFPage_InsertObject(Page, path);
            }
        }
    };

    // First layer
    {
        const auto col{ static_cast<dla::tvec3<uint32_t>>(colors[0] * 255.0f) };

        FPDF_PAGEOBJECT path{ FPDFPageObj_CreateNewPath(real_fx, real_fy) };
        FPDFPath_SetDrawMode(path, FPDF_FILLMODE_NONE, TRUE);
        FPDFPageObj_SetStrokeWidth(path, 1.0f);
        // FPDFPageObj_SetDashArray(path, dash_ptn, 1, 1.0f);
        FPDFPath_LineTo(path, real_tx, real_ty);
        FPDFPageObj_SetStrokeColor(path, col.r, col.g, col.b, 255);
        FPDFPage_InsertObject(Page, path);
    }

    // Second layer with phase offset
    {
        const auto col{ static_cast<dla::tvec3<uint32_t>>(colors[1] * 255.0f) };
        draw_dashed_line(col, 2.0f, 1.0f);
    }
}

void PdfiumPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + 3_mm * dx };
    const auto ty{ y + 3_mm * dy };

    DrawDashedLine(colors, x, y, tx, y);
    DrawDashedLine(colors, x, y, x, ty);
}

void PdfiumPage::DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation)
{
    const auto real_x{ ToPdfiumPoints(x) };
    const auto real_y{ ToPdfiumPoints(y) };
    const auto real_w{ ToPdfiumPoints(w) };
    const auto real_h{ ToPdfiumPoints(h) };

    (void)real_x;
    (void)real_y;
    (void)real_w;
    (void)real_h;

    (void)image_path;
    (void)rotation;
    // HPDF_Page_DrawImage(Page, ImageCache->GetImage(image_path, rotation), real_x, real_y, real_w, real_h);
}

PdfiumImageCache::PdfiumImageCache(FPDF_DOCUMENT document)
    : Document{ document }
{
}

FPDF_PAGEOBJECT PdfiumImageCache::GetImage(fs::path image_path, Image::Rotation rotation)
{
    const auto it{
        std::ranges::find_if(image_cache, [&](const ImageCacheEntry& entry)
                             { return entry.ImageRotation == rotation && entry.ImagePath == image_path; })
    };
    if (it != image_cache.end())
    {
        return it->PdfiumImage;
    }

    return nullptr;
    // const auto use_jpg{ CFG.PdfImageFormat == ImageFormat::Jpg };
    // const std::function<std::vector<std::byte>(const Image&)> encoder{
    //     use_jpg
    //         ? std::bind_back(&Image::EncodeJpg, CFG.JpgQuality)
    //         : std::bind_back(&Image::EncodePng, std::optional{ 0 })
    // };
    // const auto loader{
    //     use_jpg
    //         ? &HPDF_LoadJpegImageFromMem
    //         : &HPDF_LoadPngImageFromMem
    // };

    // const Image loaded_image{ Image::Read(image_path).Rotate(rotation) };
    // const auto encoded_image{ encoder(loaded_image) };
    // const auto libharu_image{
    //     loader(Document,
    //            reinterpret_cast<const HPDF_BYTE*>(encoded_image.data()),
    //            static_cast<HPDF_UINT>(encoded_image.size())),
    // };
    // image_cache.push_back({
    //     std::move(image_path),
    //     rotation,
    //     libharu_image,
    // });
    // return libharu_image;
}

PdfiumDocument::PdfiumDocument(PrintFn print_fn)
    : PrintFunction{ std::move(print_fn) }
{
    Document = FPDF_CreateNewDocument();

    ImageCache = std::make_unique<PdfiumImageCache>(Document);
}
PdfiumDocument::~PdfiumDocument()
{
    for (PdfiumPage& page : Pages)
    {
        FPDF_ClosePage(page.Page);
    }
    FPDF_CloseDocument(Document);
}

PdfiumPage* PdfiumDocument::NextPage(Size page_size)
{
    auto& new_page{ Pages.emplace_back() };
    new_page.Page = FPDFPage_New(Document,
                                 static_cast<int>(Pages.size() - 1),
                                 ToPdfiumPoints(page_size.x),
                                 ToPdfiumPoints(page_size.y));
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

class PdfWriter : public FPDF_FILEWRITE
{
  public:
    PdfWriter(const fs::path& path)
        : File{ path, std::ofstream::binary }
    {
        FPDF_FILEWRITE::version = 1;
        FPDF_FILEWRITE::WriteBlock = &PdfWriter::Write;
    }

  private:
    static int Write(FPDF_FILEWRITE* file_write,
                     const void* data,
                     unsigned long data_size)
    {
        PdfWriter& self{ *static_cast<PdfWriter*>(file_write) };
        self.File.write(static_cast<const char*>(data), data_size);
        return TRUE;
    }

    std::ofstream File;
};

std::optional<fs::path> PdfiumDocument::Write(fs::path path)
{
    for (PdfiumPage& page : Pages)
    {
        FPDFPage_GenerateContent(page.Page);
    }

    const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    PdfWriter writer{ pdf_path };

    PPP_LOG_WITH(PrintFunction, "Saving to {}...", pdf_path.string());
    FPDF_SaveAsCopy(Document, &writer, FPDF_NO_INCREMENTAL);
    return pdf_path;
}
