#include <ppp/pdf/haru_backend.hpp>

#include <QStandardPaths>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

inline HPDF_REAL ToHaruReal(Length l)
{
    return static_cast<HPDF_REAL>(l / 1_pts);
}

void HaruPdfPage::DrawSolidLine(LineData data, LineStyle style)
{
    const auto& fx{ data.m_From.x };
    const auto& fy{ data.m_From.y };
    const auto& tx{ data.m_To.x };
    const auto& ty{ data.m_To.y };

    const auto real_fx{ ToHaruReal(fx) };
    const auto real_fy{ ToHaruReal(fy) };
    const auto real_tx{ ToHaruReal(tx) };
    const auto real_ty{ ToHaruReal(ty) };
    const auto line_width{ ToHaruReal(style.m_Thickness) };

    HPDF_Page_SetLineWidth(m_Page, line_width);
    HPDF_Page_SetRGBStroke(m_Page, style.m_Color.r, style.m_Color.g, style.m_Color.b);
    HPDF_Page_MoveTo(m_Page, real_fx, real_fy);
    HPDF_Page_LineTo(m_Page, real_tx, real_ty);
    HPDF_Page_Stroke(m_Page);
}

void HaruPdfPage::DrawDashedLine(LineData data, DashedLineStyle style)
{
    if (style.m_Color == style.m_SecondColor)
    {
        DrawSolidLine(data, style);
        return;
    }

    const auto& fx{ data.m_From.x };
    const auto& fy{ data.m_From.y };
    const auto& tx{ data.m_To.x };
    const auto& ty{ data.m_To.y };

    const auto real_fx{ ToHaruReal(fx) };
    const auto real_fy{ ToHaruReal(fy) };
    const auto real_tx{ ToHaruReal(tx) };
    const auto real_ty{ ToHaruReal(ty) };
    const auto line_width{ ToHaruReal(style.m_Thickness) };
    const auto dash_size{ ToHaruReal(style.m_DashSize) };

    HPDF_Page_SetLineWidth(m_Page, line_width);

    const HPDF_REAL dash_ptn[]{ dash_size };

    // First layer
    HPDF_Page_SetDash(m_Page, dash_ptn, 1, 0.0f);
    HPDF_Page_SetRGBStroke(m_Page, style.m_Color.r, style.m_Color.g, style.m_Color.b);
    HPDF_Page_MoveTo(m_Page, real_fx, real_fy);
    HPDF_Page_LineTo(m_Page, real_tx, real_ty);
    HPDF_Page_Stroke(m_Page);

    // Second layer with phase offset
    HPDF_Page_SetDash(m_Page, dash_ptn, 1, dash_size);
    HPDF_Page_SetRGBStroke(m_Page, style.m_SecondColor.r, style.m_SecondColor.g, style.m_SecondColor.b);
    HPDF_Page_MoveTo(m_Page, real_fx, real_fy);
    HPDF_Page_LineTo(m_Page, real_tx, real_ty);
    HPDF_Page_Stroke(m_Page);
}

void HaruPdfPage::DrawImage(ImageData data)
{
    const auto& image_path{ data.m_Path };
    const auto& rotation{ data.m_Rotation };
    const auto& x{ data.m_Pos.x };
    const auto& y{ data.m_Pos.y };
    const auto& w{ data.m_Size.x };
    const auto& h{ data.m_Size.y };

    const auto real_x{ ToHaruReal(x) };
    const auto real_y{ ToHaruReal(y) };
    const auto real_w{ ToHaruReal(w) };
    const auto real_h{ ToHaruReal(h) };
    HPDF_Page_DrawImage(m_Page, m_ImageCache->GetImage(image_path, rotation), real_x, real_y, real_w, real_h);
}

void HaruPdfPage::DrawText(std::string_view text, TextBoundingBox bounding_box)
{
    const auto left{ ToHaruReal(bounding_box.m_TopLeft.x) };
    const auto top{ ToHaruReal(bounding_box.m_TopLeft.y) };
    const auto right{ ToHaruReal(bounding_box.m_BottomRight.x) };
    const auto bottom{ ToHaruReal(bounding_box.m_BottomRight.y) };

    HPDF_Page_SetFontAndSize(m_Page, m_Document->GetFont(), 12);

    char text_cpy[1024]{};
    text.copy(text_cpy, std::min(text.size(), sizeof(text_cpy) - 1));
    HPDF_Page_BeginText(m_Page);
    HPDF_Page_TextRect(m_Page, left, top, right, bottom, text_cpy, HPDF_TALIGN_CENTER, nullptr);
    HPDF_Page_EndText(m_Page);
}

HaruPdfImageCache::HaruPdfImageCache(HaruPdfDocument& document, const Project& project)
    : m_Document{ document }
    , m_Project{ project }
{
}

HPDF_Image HaruPdfImageCache::GetImage(fs::path image_path, Image::Rotation rotation)
{
    {
        std::lock_guard lock{ m_Mutex };
        const auto it{
            std::ranges::find_if(m_Cache, [&](const ImageCacheEntry& entry)
                                 { return entry.m_ImageRotation == rotation && entry.m_ImagePath == image_path; })
        };
        if (it != m_Cache.end())
        {
            return it->m_HaruImage;
        }
    }

    const bool rounded_corners{
        m_Project.m_Data.m_Corners == CardCorners::Rounded &&
        m_Project.m_Data.m_BleedEdge == 0_mm
    };
    const auto card_size{ m_Project.CardSize() };
    const auto corner_radius{
        rounded_corners
            ? m_Project.CardCornerRadius()
            : 0_mm,
    };

    const auto use_png{ g_Cfg.m_PdfImageFormat == ImageFormat::Png };
    // clang-format off
    const std::function<std::vector<std::byte>(const Image&)> encoder{
        use_png         ? [](const Image& image)
                          { return image.EncodePng(std::optional{ 0 }); } :
        rounded_corners ? [](const Image& image)
                          { return image.ApplyAlpha({ 255, 255, 255 }).EncodeJpg(g_Cfg.m_JpgQuality); }
                        : [](const Image& image)
                          { return image.EncodeJpg(g_Cfg.m_JpgQuality); }
    };
    // clang-format on

    const Image loaded_image{
        Image::Read(image_path)
            .RoundCorners(card_size, corner_radius)
            .Rotate(rotation)
    };

    const auto encoded_image{ encoder(loaded_image) };
    const auto libharu_image{
        m_Document.MakeImage(use_png, encoded_image)
    };

    std::lock_guard lock{ m_Mutex };
    m_Cache.push_back({
        std::move(image_path),
        rotation,
        libharu_image,
    });
    return libharu_image;
}

HaruPdfDocument::HaruPdfDocument(const Project& project)
    : m_Project{ project }
{
    static constexpr auto c_ErrorHandler{
        [](HPDF_STATUS error_no,
           HPDF_STATUS detail_no,
           [[maybe_unused]] void* user_data)
        {
            LogError("HaruPDF ERROR: error_no={}, detail_no={}", error_no, detail_no);
        }
    };

    m_Document = HPDF_New(c_ErrorHandler, nullptr);
    HPDF_SetCompressionMode(m_Document, HPDF_COMP_ALL);

    m_ImageCache = std::make_unique<HaruPdfImageCache>(*this, project);
}
HaruPdfDocument::~HaruPdfDocument()
{
    HPDF_Free(m_Document);
}

void HaruPdfDocument::ReservePages(size_t pages)
{
    std::lock_guard lock{ m_Mutex };
    m_Pages.reserve(pages);
}

HaruPdfPage* HaruPdfDocument::NextPage()
{
    const auto page_size{ m_Project.ComputePageSize() };

    std::lock_guard lock{ m_Mutex };
    auto& new_page{ m_Pages.emplace_back() };
    new_page.m_Page = HPDF_AddPage(m_Document);
    new_page.m_Document = this;
    HPDF_Page_SetWidth(new_page.m_Page, ToHaruReal(page_size.x));
    HPDF_Page_SetHeight(new_page.m_Page, ToHaruReal(page_size.y));
    new_page.m_ImageCache = m_ImageCache.get();
    return &new_page;
}

fs::path HaruPdfDocument::Write(fs::path path)
{
    const fs::path pdf_path{ fs::path{ path }.replace_extension(".pdf") };
    const auto pdf_path_string{ pdf_path.string() };
    LogInfo("Saving to {}...", pdf_path_string);

    std::lock_guard lock{ m_Mutex };
    HPDF_SaveToFile(m_Document, pdf_path_string.c_str());
    return pdf_path;
}
HPDF_Font HaruPdfDocument::GetFont()
{
    std::lock_guard lock{ m_Mutex };
    if (m_Font == nullptr)
    {
        const auto arial_path{ QStandardPaths::locate(QStandardPaths::FontsLocation, "arial.ttf") };
        const auto arial_font_name{ HPDF_LoadTTFontFromFile(m_Document, arial_path.toStdString().c_str(), true) };
        m_Font = HPDF_GetFont(m_Document, arial_font_name, HPDF_ENCODING_FONT_SPECIFIC);
    }
    return m_Font;
}

HPDF_Image HaruPdfDocument::MakeImage(bool use_png, std::span<const std::byte> encoded_image)
{
    const auto loader{
        use_png
            ? &HPDF_LoadPngImageFromMem
            : &HPDF_LoadJpegImageFromMem
    };

    std::lock_guard lock{ m_Mutex };
    return loader(m_Document,
                  reinterpret_cast<const HPDF_BYTE*>(encoded_image.data()),
                  static_cast<HPDF_UINT>(encoded_image.size()));
}
