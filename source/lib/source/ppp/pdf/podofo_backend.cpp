#include <ppp/pdf/podofo_backend.hpp>

#include <dla/vector_math.h>

#include <podofo/podofo.h>

#include <ppp/pdf/util.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

inline double ToPoDoFoPoints(Length l)
{
    return static_cast<double>(l / 1_pts);
}

auto PoDoFoDocument::AquireDocumentLock()
{
    return std::lock_guard{ m_Mutex };
}

auto Save(PoDoFo::PdfPainter& painter)
{
    painter.Save();
    return AtScopeExit{
        [&painter]
        {
            painter.Restore();
        }
    };
}

PoDoFoPage::PoDoFoPage(PoDoFo::PdfPage* page,
                       PoDoFo::PdfPainter* painter,
                       PoDoFoDocument* document,
                       PoDoFoImageCache* image_cache)
    : m_Page{ page }
    , m_Painter{ painter }
    , m_Document{ document }
    , m_ImageCache{ image_cache }
{
    m_Painter->SetCanvas(*m_Page, PoDoFo::PdfPainterFlags::NoSaveRestorePrior);
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

    auto save{ Save(*m_Painter) };
    m_Painter->GraphicsState.SetLineWidth(line_width);
    m_Painter->GraphicsState.SetStrokingColor(col);
    m_Painter->SetStrokeStyle(PoDoFo::PdfStrokeStyle::Solid);
    m_Painter->DrawLine(real_fx, real_fy, real_tx, real_ty);
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

    auto save{ Save(*m_Painter) };
    m_Painter->GraphicsState.SetLineWidth(line_width);

    // First layer
    {
        const std::array dash{ dash_size, dash_size };
        m_Painter->SetStrokeStyle(dash, 0.0);

        const PoDoFo::PdfColor col{ style.m_Color.r, style.m_Color.g, style.m_Color.b };
        m_Painter->GraphicsState.SetStrokingColor(col);

        m_Painter->DrawLine(real_fx, real_fy, real_tx, real_ty);
    }

    // Second layer with phase offset
    {
        const std::array dash{ dash_size, dash_size };
        m_Painter->SetStrokeStyle(dash, dash_size);

        const PoDoFo::PdfColor col{ style.m_SecondColor.r, style.m_SecondColor.g, style.m_SecondColor.b };
        m_Painter->GraphicsState.SetStrokingColor(col);

        m_Painter->DrawLine(real_fx, real_fy, real_tx, real_ty);
    }
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

    auto save{ Save(*m_Painter) };

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

        const PoDoFo::Rect clip_rect{
            real_cx,
            real_cy,
            real_cw,
            real_ch,
        };
        m_Painter->SetClipRect(clip_rect);
    }

    // Does this API access the document or mutate the image (despite const-declaration)?
    // I honestly don't know, so better safe than sorry
    auto lock{ m_Document->AquireDocumentLock() };

    m_Painter->DrawImage(*image, real_x, real_y, w_scale, h_scale);
}

void PoDoFoPage::DrawText(TextData data)
{
    const auto& bb{ data.m_BoundingBox };
    const PoDoFo::Rect rect{
        ToPoDoFoPoints(bb.m_TopLeft.x),
        ToPoDoFoPoints(bb.m_BottomRight.y),
        ToPoDoFoPoints(bb.m_BottomRight.x - bb.m_TopLeft.x),
        ToPoDoFoPoints(bb.m_TopLeft.y - bb.m_BottomRight.y),
    };
    const PoDoFo::PdfString str{ data.m_Text };
    auto& font{ m_Document->GetFont() };

    auto save{ Save(*m_Painter) };
    m_Painter->TextState.SetFont(font, 12);

    if (data.m_Backdrop.has_value())
    {
        const PoDoFo::PdfColor col{
            data.m_Backdrop.value().r,
            data.m_Backdrop.value().g,
            data.m_Backdrop.value().b,
        };

        const auto& text_state{ m_Painter->TextState.GetState() };
        const auto get_string_length{
            [&](const auto& str)
            { return text_state.Font->GetStringLength(str, text_state); }
        };

        const auto lines{ text_state.SplitTextAsLines(str, rect.Width) };
        auto widths{ lines |
                     std::views::transform(get_string_length) };
        const auto max_width{ *std::ranges::max_element(widths) };
        const auto width_reduce{ (rect.Width - max_width) };
        const PoDoFo::Rect backdrop_rect{
            rect.GetLeft() + width_reduce / 2,
            rect.GetBottom(),
            rect.Width - width_reduce,
            rect.Height,
        };

        auto save_backdrop{ Save(*m_Painter) };
        m_Painter->GraphicsState.SetNonStrokingColor(col);
        m_Painter->DrawRectangle(backdrop_rect, PoDoFo::PdfPathDrawMode::Fill);
    }

    PoDoFo::PdfDrawTextMultiLineParams params{
        .HorizontalAlignment = PoDoFo::PdfHorizontalAlignment::Center,
        .VerticalAlignment = PoDoFo::PdfVerticalAlignment::Center,
    };
    m_Painter->DrawTextMultiLine(str,
                                 rect,
                                 params);
}

void PoDoFoPage::Finish()
{
    m_Painter->FinishDrawing();
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
        g_Cfg.m_PdfImageCompression == ImageCompression::Lossy ||
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
    podofo_image.get()->LoadFromBuffer(
        PoDoFo::bufferview{
            reinterpret_cast<const char*>(encoded_image.data()),
            encoded_image.size(),
        });

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
        m_BaseDocument->Load(full_path.string());

        {
            // Some pdf files, most likely written by Cairo, have a transform at the start of the page
            // that is not wrapped with q/Q, since the assumption is that the pdf won't be edited. To
            // be able to put new stuff into the pdf we wrap the whole page in a q/Q pair
            auto& page{ m_BaseDocument->GetPages().GetPageAt(0) };

            auto& contents{ page.GetContents()->GetObject() };
            if (contents.IsArray())
            {
                auto* owner{ page.GetObject().GetDocument() };
                auto& objects{ owner->GetObjects() };

                auto& save_graphics_state{ objects.CreateDictionaryObject() };
                save_graphics_state.GetOrCreateStream().SetData("q");
                auto& restore_graphics_state{ objects.CreateDictionaryObject() };
                restore_graphics_state.GetOrCreateStream().SetData("Q");

                auto& array{ contents.GetArray() };
                array.insert(array.begin(), save_graphics_state.GetIndirectReference());
                array.insert(array.end(), restore_graphics_state.GetIndirectReference());
            }
            else
            {
                auto* contents_stream{ contents.GetStream() };

                const auto stream_data{ contents_stream->GetCopy() };

                auto out_stream{ contents_stream->GetOutputStream() };
                out_stream.Write("q ");
                out_stream.Write(stream_data);
                out_stream.Write(" Q");
            }
        }
    }
}

void PoDoFoDocument::ReservePages(size_t pages)
{
    auto lock{ AquireDocumentLock() };
    m_Pages.reserve(pages);
}

PoDoFoPage* PoDoFoDocument::NextPage()
{
    auto lock{ AquireDocumentLock() };

    const int new_page_idx{ static_cast<int>(m_Pages.size()) };
    PoDoFo::PdfPage* page{ nullptr };
    if (m_BaseDocument != nullptr)
    {
        m_Document.GetPages().InsertDocumentPageAt(new_page_idx, *m_BaseDocument, 0);
        page = &m_Document
                    .GetPages()
                    .GetPageAt(new_page_idx);

        const bool is_backside{ m_Project.m_Data.m_BacksideEnabled &&
                                m_Pages.size() % 2 == 0 };
        if (is_backside)
        {
            const auto dx{ ToPoDoFoPoints(m_Project.m_Data.m_BacksideOffset.x) };
            const auto dy{ ToPoDoFoPoints(m_Project.m_Data.m_BacksideOffset.y) };

            const auto transform_str{
                [&]()
                {
                    std::ostringstream transform_stream;
                    transform_stream.flags(std::ios_base::fixed);
                    transform_stream.precision(15);
                    transform_stream << 1.0 << " " // scale-x
                                     << 0.0 << " " // rot-1
                                     << 0.0 << " " // rot-2
                                     << 1.0 << " " // scale-y
                                     << -dx << " " // trans-x
                                     << dy << " "  // trans-y
                                     << "cm " << std::endl;
                    return PoDoFo::PdfString{ transform_stream.str() };
                }(),
            };

            auto& contents{ page->GetContents()->GetObject() };
            if (contents.IsArray())
            {
                auto& owner{ page->GetDocument() };
                auto& objects{ owner.GetObjects() };

                auto& save_graphics_state{ objects.CreateDictionaryObject() };
                save_graphics_state.GetOrCreateStream().SetData("q");
                auto& transform_state{ objects.CreateDictionaryObject() };
                transform_state.GetOrCreateStream().SetData(transform_str.GetString());
                auto& restore_graphics_state{ objects.CreateDictionaryObject() };
                restore_graphics_state.GetOrCreateStream().SetData("q");

                auto& array{ contents.GetArray() };
                array.insert(array.begin(), transform_state.GetIndirectReference());
                array.insert(array.begin(), save_graphics_state.GetIndirectReference());
                array.insert(array.end(), restore_graphics_state.GetIndirectReference());
            }
            else
            {
                auto* contents_stream{ contents.GetStream() };
                const auto stream_data{ contents_stream->GetCopy() };

                auto out_stream{ contents_stream->GetOutputStream() };
                out_stream.Write("q ");
                out_stream.Write(transform_str);
                out_stream.Write(stream_data);
                out_stream.Write(" Q");
            }
        }
    }
    else
    {
        const auto page_size{ m_Project.ComputePageSize() };
        page = &m_Document.GetPages().CreatePageAt(
            new_page_idx,
            PoDoFo::Rect(
                0.0,
                0.0,
                ToPoDoFoPoints(page_size.x),
                ToPoDoFoPoints(page_size.y)));
    }

    auto* painter{ m_Painters.emplace_back(new PoDoFo::PdfPainter).get() };

    m_Pages.push_back(PoDoFoPage{ page, painter, this, m_ImageCache.get() });
    return &m_Pages.back();
}

fs::path PoDoFoDocument::Write(fs::path path)
{
    try
    {
        const auto pdf_path{ fs::path{ path }.replace_extension(".pdf") };
        const auto pdf_path_string{ pdf_path.string() };
        LogInfo("Saving to {}...", pdf_path_string);

        auto lock{ AquireDocumentLock() };

        if (g_Cfg.m_DeterminsticPdfOutput)
        {
            auto& trailer{ m_Document.GetTrailer() };
            const auto& ref = trailer.GetDictionary().GetKey("Info")->GetReference();
            auto* obj = m_Document.GetObjects().GetObject(ref);
            obj->GetDictionary().RemoveKey("CreationDate");

            m_Document.Save(pdf_path.string(), PoDoFo::PdfSaveOptions::NoMetadataUpdate);
        }
        else
        {
            m_Document.Save(pdf_path.string());
        }

        return pdf_path;
    }
    catch (const PoDoFo::PdfError& e)
    {
        // Rethrow as a std::exception so the agnostic code can catch it
        throw std::logic_error{ e.what() };
    }
}

PoDoFo::PdfFont& PoDoFoDocument::GetFont()
{
    auto lock{ AquireDocumentLock() };
    return m_Document
        .GetFonts()
        .GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
}

std::unique_ptr<PoDoFo::PdfImage> PoDoFoDocument::MakeImage()
{
    auto lock{ AquireDocumentLock() };
    return m_Document.CreateImage();
}
