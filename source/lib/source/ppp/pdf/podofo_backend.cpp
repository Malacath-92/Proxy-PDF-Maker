#include <ppp/pdf/podofo_backend.hpp>

#include <dla/vector_math.h>

#include <podofo/base/PdfLocale.h>
#include <podofo/podofo.h>

#include <ppp/pdf/util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

inline double ToPoDoFoPoints(Length l)
{
    return static_cast<double>(l / 1_pts);
}

void PoDoFoPage::DrawSolidLine(LineData data, LineStyle style)
{
    const auto& fx{ data.m_From.x };
    const auto& fy{ data.m_From.y };
    const auto& tx{ data.m_To.x };
    const auto& ty{ data.m_To.y };

    const auto real_fx{ ToPoDoFoPoints(fx) };
    const auto real_fy{ ToPoDoFoPoints(fy) };
    const auto real_tx{ ToPoDoFoPoints(tx) };
    const auto real_ty{ ToPoDoFoPoints(ty) };
    const auto line_width{ ToPoDoFoPoints(style.m_Thickness) };
    const PoDoFo::PdfColor col{ style.m_Color.r, style.m_Color.g, style.m_Color.b };

    PoDoFo::PdfPainter painter;
    painter.SetPage(m_Page);
    painter.Save();

    painter.SetStrokeWidth(line_width);
    painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Solid, nullptr, false, 1.0, false);
    painter.SetStrokingColor(col);
    painter.DrawLine(real_fx, real_fy, real_tx, real_ty);

    painter.Restore();
    painter.FinishPage();
}

void PoDoFoPage::DrawDashedLine(LineData data, DashedLineStyle style)
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

    const auto real_fx{ ToPoDoFoPoints(fx) };
    const auto real_fy{ ToPoDoFoPoints(fy) };
    const auto real_tx{ ToPoDoFoPoints(tx) };
    const auto real_ty{ ToPoDoFoPoints(ty) };
    const auto line_width{ ToPoDoFoPoints(style.m_Thickness) };
    const auto final_dash_size{ ComputeFinalDashSize(dla::distance(data.m_To, data.m_From), style.m_TargetDashSize) };
    const auto dash_size{ ToPoDoFoPoints(final_dash_size) };

    PoDoFo::PdfPainter painter;
    painter.SetPage(m_Page);
    painter.Save();

    painter.SetStrokeWidth(line_width);

    // First layer
    {
        const std::string dash{ fmt::format("[{0:.2f} {0:.2f}] 0", dash_size) };
        painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Custom, dash.c_str(), false, 1.0, false);

        const PoDoFo::PdfColor col{ style.m_Color.r, style.m_Color.g, style.m_Color.b };
        painter.SetStrokingColor(col);

        painter.DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    // Second layer with phase offset
    {
        const std::string dash{ fmt::format("[{0:.2f} {0:.2f}] {0:.2f}", dash_size) };
        painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Custom, dash.c_str(), false, 1.0, false);

        const PoDoFo::PdfColor col{ style.m_SecondColor.r, style.m_SecondColor.g, style.m_SecondColor.b };
        painter.SetStrokingColor(col);

        painter.DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    painter.Restore();
    painter.FinishPage();
}

void PoDoFoPage::DrawImage(ImageData data)
{
    const auto& image_path{ data.m_Path };
    const auto& rotation{ data.m_Rotation };
    const auto& x{ data.m_Pos.x };
    const auto& y{ data.m_Pos.y };
    const auto& w{ data.m_Size.x };
    const auto& h{ data.m_Size.y };

    const auto real_x{ ToPoDoFoPoints(x) };
    const auto real_y{ ToPoDoFoPoints(y) };
    const auto real_w{ ToPoDoFoPoints(w) };
    const auto real_h{ ToPoDoFoPoints(h) };

    auto* image{ m_ImageCache->GetImage(image_path, rotation) };
    const auto w_scale{ real_w / image->GetWidth() };
    const auto h_scale{ real_h / image->GetHeight() };

    PoDoFo::PdfPainter painter;
    painter.SetPage(m_Page);
    painter.Save();

    if (data.m_ClipRect.has_value())
    {
        const auto& cx{ data.m_ClipRect.value().m_Position.x };
        const auto& cy{ data.m_ClipRect.value().m_Position.y };
        const auto& cw{ data.m_ClipRect.value().m_Size.x };
        const auto& ch{ data.m_ClipRect.value().m_Size.y };

        const auto real_cx{ ToPoDoFoPoints(cx) };
        const auto real_cy{ ToPoDoFoPoints(cy) };
        const auto real_cw{ ToPoDoFoPoints(cw) };
        const auto real_ch{ ToPoDoFoPoints(ch) };

        const PoDoFo::PdfRect clip_rect{
            real_cx,
            real_cy,
            real_cw,
            real_ch,
        };
        painter.SetClipRect(clip_rect);
    }

    painter.DrawImage(real_x, real_y, image, w_scale, h_scale);

    painter.Restore();
    painter.FinishPage();
}

void PoDoFoPage::DrawText(TextData data)
{
    const auto& bb{ data.m_BoundingBox };
    const PoDoFo::PdfRect rect{
        ToPoDoFoPoints(bb.m_TopLeft.x),
        ToPoDoFoPoints(bb.m_BottomRight.y),
        ToPoDoFoPoints(bb.m_BottomRight.x - bb.m_TopLeft.x),
        ToPoDoFoPoints(bb.m_TopLeft.y - bb.m_BottomRight.y),
    };
    const PoDoFo::PdfString str{
        data.m_Text.data(),
        static_cast<PoDoFo::pdf_long>(data.m_Text.size()),
    };
    auto* font{ m_Document->GetFont() };

    PoDoFo::PdfPainter painter;
    painter.SetPage(m_Page);

    if (data.m_Backdrop.has_value())
    {
        painter.Save();
        painter.SetFont(font);

        const PoDoFo::PdfColor col{
            data.m_Backdrop.value().r,
            data.m_Backdrop.value().g,
            data.m_Backdrop.value().b,
        };
        painter.SetColor(col);

        const auto lines{ painter.GetMultiLineTextAsLines(rect.GetWidth(), str) };
        auto widths{ lines |
                     std::views::transform([font](const auto& str)
                                           { return font->GetFontMetrics()->StringWidth(str); }) };
        const auto max_width{ *std::ranges::max_element(widths) };
        const auto width_reduce{ (rect.GetWidth() - max_width) };
        const PoDoFo::PdfRect backdrop_rect{
            rect.GetLeft() + width_reduce / 2,
            rect.GetBottom(),
            rect.GetWidth() - width_reduce,
            rect.GetHeight(),
        };
        painter.Rectangle(backdrop_rect);
        painter.Fill();

        painter.Restore();
    }

    painter.Save();

    painter.SetStrokeStyle(PoDoFo::ePdfStrokeStyle_Solid, nullptr, false, 1.0, false);
    painter.SetFont(font);
    painter.DrawMultiLineText(rect,
                              str,
                              PoDoFo::ePdfAlignment_Center,
                              PoDoFo::ePdfVerticalAlignment_Center);

    painter.Restore();
    painter.FinishPage();
}

PoDoFoImageCache::PoDoFoImageCache(PoDoFoDocument& document, const Project& project)
    : m_Document{ document }
    , m_Project{ project }
{
}

PoDoFo::PdfImage* PoDoFoImageCache::GetImage(fs::path image_path, Image::Rotation rotation)
{
    auto find_existing_image{
        [&]() -> PoDoFo::PdfImage*
        {
            const auto it{
                std::ranges::find_if(m_Cache, [&](const ImageCacheEntry& entry)
                                     { return entry.m_ImageRotation == rotation && entry.m_ImagePath == image_path; })
            };
            if (it != m_Cache.end())
            {
                return it->m_PoDoFoImage.get();
            }
            return nullptr;
        }
    };
    {
        std::lock_guard lock{ m_Mutex };
        if (auto* img{ find_existing_image() })
        {
            return img;
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

    const auto use_jpg{
        g_Cfg.m_PdfImageCompression == ImageCompression::Lossless ||
        (g_Cfg.m_PdfImageCompression == ImageCompression::AsIs &&
         std::ranges::contains(g_LossyImageExtensions, image_path.extension()))
    };
    const auto use_png{ !use_jpg };
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
    const auto loader{
        use_png
            ? &PoDoFo::PdfImage::LoadFromPngData
            : &PoDoFo::PdfImage::LoadFromJpegData
    };

    const Image loaded_image{
        Image::Read(image_path)
            .RoundCorners(card_size, corner_radius)
            .Rotate(rotation)
    };

    const auto encoded_image{ encoder(loaded_image) };

    std::lock_guard lock{ m_Mutex };
    if (auto* img{ find_existing_image() })
    {
        // If we end up here we discard all the work we did before
        // but if we don't we will destroy an existing image and probably
        // corrup the pdf
        return img;
    }

    std::unique_ptr podofo_image{ m_Document.MakeImage() };
    (podofo_image.get()->*loader)(reinterpret_cast<const unsigned char*>(encoded_image.data()), encoded_image.size());

    m_Cache.push_back({
        std::move(image_path),
        rotation,
        std::move(podofo_image),
    });
    return m_Cache.back().m_PoDoFoImage.get();
}

PoDoFoDocument::PoDoFoDocument(const Project& project)
    : m_Project{ project }
    , m_BaseDocument{
        project.m_Data.m_PageSize == Config::c_BasePDFSize && LoadPdfSize(project.m_Data.m_BasePdf + ".pdf")
            ? new PoDoFo::PdfMemDocument
            : nullptr
    }
{
    m_ImageCache = std::make_unique<PoDoFoImageCache>(*this, project);

    if (m_BaseDocument != nullptr)
    {
        const fs::path full_path{ "./res/base_pdfs" / fs::path{ project.m_Data.m_BasePdf + ".pdf" } };
        m_BaseDocument->Load(full_path.c_str());

        {
            // Some pdf files, most likely written by Cairo, have a transform at the start of the page
            // that is not wrapped with q/Q, since the assumption is that the pdf won't be edited. To
            // be able to put new stuff into the pdf we wrap the whole page in a q/Q pair
            PoDoFo::PdfPage* page{ m_BaseDocument->GetPage(0) };

            auto* contents{ page->GetContents() };
            if (contents->IsArray())
            {
                auto* owner{ page->GetObject()->GetOwner() };

                auto* save_graphics_state{ owner->CreateObject() };
                save_graphics_state->GetStream()->Set("q");

                auto* restore_graphics_state{ owner->CreateObject() };
                restore_graphics_state->GetStream()->Set("Q");

                auto& array{ contents->GetArray() };
                array.insert(array.begin(), save_graphics_state->Reference());
                array.push_back(restore_graphics_state->Reference());
            }
            else
            {
                auto* contents_stream{ contents->GetStream() };

                PoDoFo::pdf_long stream_size{};
                char* stream_data{};
                contents_stream->GetFilteredCopy(&stream_data, &stream_size);

                contents_stream->BeginAppend(true);
                contents_stream->Append("q ");
                contents_stream->Append(stream_data, stream_size);
                contents_stream->Append(" Q");
                contents_stream->EndAppend();

                PoDoFo::podofo_free(stream_data);
            }
        }
    }
}

void PoDoFoDocument::ReservePages(size_t pages)
{
    std::lock_guard lock{ m_Mutex };
    m_Pages.reserve(pages);
}

PoDoFoPage* PoDoFoDocument::NextPage()
{
    std::lock_guard lock{ m_Mutex };

    auto& new_page{ m_Pages.emplace_back() };
    const int new_page_idx{ static_cast<int>(m_Pages.size() - 1) };
    if (m_BaseDocument != nullptr)
    {
        new_page.m_Page = m_Document.InsertExistingPageAt(
                                        *m_BaseDocument,
                                        0,
                                        new_page_idx)
                              .GetPage(new_page_idx);

        const bool is_backside{ m_Project.m_Data.m_BacksideEnabled &&
                                m_Pages.size() % 2 == 0 };
        if (is_backside)
        {
            auto* page{ new_page.m_Page };

            const auto dx{ ToPoDoFoPoints(m_Project.m_Data.m_BacksideOffset.x) };
            const auto dy{ ToPoDoFoPoints(m_Project.m_Data.m_BacksideOffset.y) };

            std::ostringstream stream;
            stream.flags(std::ios_base::fixed);
            stream.precision(15);
            PoDoFo::PdfLocaleImbue(stream);
            stream << 1.0 << " " // scale-x
                   << 0.0 << " " // rot-1
                   << 0.0 << " " // rot-2
                   << 1.0 << " " // scale-y
                   << -dx << " " // trans-x
                   << dy << " "  // trans-y
                   << "cm " << std::endl;

            auto contents{ page->GetContents() };
            if (contents->IsArray())
            {
                auto* owner{ page->GetObject()->GetOwner() };

                auto* save_graphics_state{ owner->CreateObject() };
                save_graphics_state->GetStream()->Set("q");

                auto* transform_state{ owner->CreateObject() };
                auto* transform_stream{ transform_state->GetStream() };
                transform_stream->BeginAppend();
                transform_stream->Append(stream.str());
                transform_stream->EndAppend();

                auto* restore_graphics_state{ owner->CreateObject() };
                restore_graphics_state->GetStream()->Set("Q");

                auto& array{ contents->GetArray() };
                array.insert(array.begin(), transform_state->Reference());
                array.insert(array.begin(), save_graphics_state->Reference());
                array.push_back(restore_graphics_state->Reference());
            }
            else
            {
                auto* contents_stream{ contents->GetStream() };

                PoDoFo::pdf_long stream_size{};
                char* stream_data{};
                contents_stream->GetFilteredCopy(&stream_data, &stream_size);

                contents_stream->BeginAppend(true);
                contents_stream->Append("q ");
                contents_stream->Append(stream.str());
                contents_stream->Append(stream_data, stream_size);
                contents_stream->Append(" Q");
                contents_stream->EndAppend();

                PoDoFo::podofo_free(stream_data);
            }
        }
    }
    else
    {
        const auto page_size{ m_Project.ComputePageSize() };
        new_page.m_Page = m_Document.InsertPage(
            PoDoFo::PdfRect(
                0.0,
                0.0,
                ToPoDoFoPoints(page_size.x),
                ToPoDoFoPoints(page_size.y)),
            new_page_idx);
    }
    new_page.m_Document = this;
    new_page.m_ImageCache = m_ImageCache.get();
    return &new_page;
}

fs::path PoDoFoDocument::Write(fs::path path)
{
    try
    {
        const auto pdf_path{ fs::path{ path }.replace_extension(".pdf") };
        const auto pdf_path_string{ pdf_path.string() };
        LogInfo("Saving to {}...", pdf_path_string);

        std::lock_guard lock{ m_Mutex };

        if (g_Cfg.m_DeterminsticPdfOutput)
        {
            auto* trailer{ m_Document.GetTrailer() };
            const auto& ref = trailer->GetDictionary().GetKey("Info")->GetReference();
            auto* obj = m_Document.GetObjects().GetObject(ref);
            obj->GetDictionary().RemoveKey("CreationDate");
        }

        m_Document.Write(pdf_path.c_str());
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
    std::lock_guard lock{ m_Mutex };
    if (m_Font == nullptr)
    {
        m_Font = m_Document.CreateFont("arial");
    }
    return m_Font;
}

std::unique_ptr<PoDoFo::PdfImage> PoDoFoDocument::MakeImage()
{
    std::lock_guard lock{ m_Mutex };
    return std::make_unique<PoDoFo::PdfImage>(&m_Document);
}
