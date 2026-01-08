#include <ppp/pdf/png_backend.hpp>

#include <opencv2/imgproc.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

inline int32_t ToPixels(Length l)
{
    return static_cast<int32_t>(std::ceil(l * g_Cfg.m_MaxDPI / 1_pix));
}

void PngPage::SetPageName(std::string_view page_name)
{
    m_PageName = fs::path{ page_name }.replace_extension().string();
}

void PngPage::DrawSolidLine(LineData data, LineStyle style)
{
    const auto& fx{ data.m_From.x };
    const auto& fy{ data.m_From.y };
    const auto& tx{ data.m_To.x };
    const auto& ty{ data.m_To.y };

    const auto real_fx{ ToPixels(fx) };
    const auto real_fy{ m_PageHeight - ToPixels(fy) };
    const auto real_tx{ ToPixels(tx) };
    const auto real_ty{ m_PageHeight - ToPixels(ty) };
    const auto real_w{ (ToPixels(style.m_Thickness) / 2) * 2 };

    const cv::Point line_from{ real_fx, real_fy };
    const cv::Point line_to{ real_tx, real_ty };
    const cv::Point delta{ line_to - line_from };

    const cv::Point perp{ delta.x == 0 ? cv::Point{ real_w / 2, 0 } : cv::Point{ 0, real_w / 2 } };
    const cv::Point from{ line_from - perp };
    const cv::Point to{ line_to + perp };

    const cv::Scalar color_cv{ style.m_Color.b * 255, style.m_Color.g * 255, style.m_Color.r * 255, 255.0f };

    cv::rectangle(TargetImage(), from, to, color_cv, cv::FILLED);
}

void PngPage::DrawDashedLine(LineData data, DashedLineStyle style)
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

    const auto real_fx{ ToPixels(fx) };
    const auto real_fy{ m_PageHeight - ToPixels(fy) };
    const auto real_tx{ ToPixels(tx) };
    const auto real_ty{ m_PageHeight - ToPixels(ty) };
    const auto real_w{ (ToPixels(style.m_Thickness) / 2) * 2 };

    const cv::Point line_from{ real_fx, real_fy };
    const cv::Point line_to{ real_tx, real_ty };
    const cv::Point delta{ line_to - line_from };

    const cv::Point perp{ delta.x == 0 ? cv::Point{ real_w / 2, 0 } : cv::Point{ 0, real_w / 2 } };
    const cv::Point from{ line_from - perp };
    const cv::Point to{ line_to + perp };

    const cv::Scalar color_a{ style.m_Color.b * 255, style.m_Color.g * 255, style.m_Color.r * 255, 255.0f };
    const cv::Scalar color_b{ style.m_SecondColor.b * 255, style.m_SecondColor.g * 255, style.m_SecondColor.r * 255, 255.0f };

    cv::rectangle(TargetImage(), from, to, color_a, cv::FILLED);

    const auto dash_size{ ComputeFinalDashSize(dla::distance(data.m_To, data.m_From), style.m_TargetDashSize) };
    const auto dash_freq{ dash_size / dla::distance(data.m_From, data.m_To) };
    for (size_t i = 0; i < static_cast<size_t>(0.5f / dash_freq); ++i)
    {
        const float alpha{ i * dash_freq * 2.0f };
        const cv::Point sub_from{ from + alpha * delta };
        const cv::Point sub_to{ from + 2 * perp + (alpha + dash_freq / 2.0f) * delta };
        cv::rectangle(TargetImage(), sub_from, sub_to, color_b, cv::FILLED);
    }
}

void PngPage::DrawImage(ImageData data)
{
    const auto& image_path{ data.m_Path };
    const auto& rotation{ data.m_Rotation };
    const auto& x{ data.m_Pos.x };
    const auto& y{ data.m_Pos.y };
    if (m_PerfectFit)
    {
        const auto card_idx_x{ static_cast<int32_t>(std::round(static_cast<float>(static_cast<float>(ToPixels(x)) / m_CardSize.x))) };
        const auto card_idx_y{ static_cast<int32_t>(std::round(static_cast<float>(static_cast<float>(ToPixels(y)) / m_CardSize.y))) };
        const auto real_w{ static_cast<int32_t>(m_CardSize.x / 1_pix) };
        const auto real_h{ static_cast<int32_t>(m_CardSize.y / 1_pix) };
        const auto real_x{ card_idx_x * static_cast<int32_t>(m_CardSize.x / 1_pix) };
        const auto real_y{ m_PageHeight - card_idx_y * static_cast<int32_t>(m_CardSize.y / 1_pix) + real_h };
        m_ImageCache->GetImage(image_path, real_w, real_h, rotation)
            .copyTo(TargetImage()(cv::Rect(real_x, real_y, real_w, real_h)));
    }
    else
    {
        const auto& w{ data.m_Size.x };
        const auto& h{ data.m_Size.y };

        const auto real_x{ ToPixels(x) };
        const auto real_y{ m_PageHeight - ToPixels(y + h) };
        const auto real_w{ ToPixels(w) };
        const auto real_h{ ToPixels(h) };
        m_ImageCache->GetImage(image_path, real_w, real_h, rotation)
            .copyTo(TargetImage()(cv::Rect(real_x, real_y, real_w, real_h)));
    }
}

void PngPage::DrawText(TextData data)
{
    const cv::String cv_text{ data.m_Text.data(), data.m_Text.size() };
    const auto font_face{ cv::FONT_HERSHEY_TRIPLEX };
    const auto font_scale{ 3.0 };
    const auto font_thickness{ 3 };
    const auto text_size{ cv::getTextSize(cv_text, font_face, font_scale, font_thickness, nullptr) };

    const auto real_width{ ToPixels(data.m_BoundingBox.m_BottomRight.x - data.m_BoundingBox.m_TopLeft.x) };
    const auto real_left{ ToPixels(data.m_BoundingBox.m_TopLeft.x) };

    const auto aligned_left{ real_left + (real_width - text_size.width) / 2 };
    const auto real_top{ m_PageHeight - ToPixels(data.m_BoundingBox.m_TopLeft.y) };
    const auto aligned_top{ real_top + text_size.height };

    if (data.m_Backdrop.has_value())
    {
        const cv::Rect rect{
            aligned_left - 5, real_top - 5, text_size.width + 10, text_size.height + 10
        };
        const cv::Scalar color{ data.m_Backdrop->b * 255, data.m_Backdrop->g * 255, data.m_Backdrop->r * 255, 255.0f };
        cv::rectangle(TargetImage(), rect, color, cv::FILLED);
    }

    const cv::Scalar black{ 0, 0, 0, 255.0f };
    cv::putText(TargetImage(),
                cv::String{ data.m_Text.data(), data.m_Text.size() },
                cv::Point{ aligned_left, aligned_top },
                font_face,
                font_scale,
                black,
                font_thickness);
}

void PngPage::RotateFutureContent(Angle angle)
{
    const auto next_angle{
        m_RotatedImages.empty() ? angle
                                : m_RotatedImages.back().m_Angle + angle
    };
    m_RotatedImages.push_back({
        cv::Mat::zeros(cv::Size{
                           static_cast<int32_t>(m_PageSize.x / 1_pix),
                           static_cast<int32_t>(m_PageSize.y / 1_pix) },
                       CV_8UC4),
        next_angle,
    });
}

void PngPage::Finish()
{
    const cv::Size page_size{ static_cast<int32_t>(m_PageSize.x / 1_pix), static_cast<int32_t>(m_PageSize.y / 1_pix) };
    const cv::Point2f center{ m_PageSize.x / 2_pix, m_PageSize.y / 2_pix };
    for (const auto& [img, angle] : m_RotatedImages)
    {
        const auto transform_matrix{ cv::getRotationMatrix2D(center, angle / 1_deg, 1.0) };

        cv::Mat rotated;
        cv::warpAffine(img, rotated, transform_matrix, page_size);

        rotated.copyTo(m_Page);
    }
}

cv::Mat& PngPage::TargetImage()
{
    return m_RotatedImages.empty() ? m_Page
                                   : m_RotatedImages.back().m_Image;
}

PngImageCache::PngImageCache(const Project& project)
    : m_Project{ project }
{
}

cv::Mat PngImageCache::GetImage(fs::path image_path, int32_t w, int32_t h, Image::Rotation rotation)
{
    {
        std::lock_guard lock{ m_Mutex };
        // clang-format off
        const auto it{
            std::ranges::find_if(m_Cache,
                                 [&](const ImageCacheEntry& entry)
                                 { return entry.m_ImageRotation == rotation &&
                                          entry.m_Width == w &&
                                          entry.m_Height == h &&
                                          entry.m_ImagePath == image_path; })
        };
        // clang-format on
        if (it != m_Cache.end())
        {
            return it->m_PngImage;
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
    const Image loaded_image{
        Image::Read(image_path)
            .RoundCorners(card_size, corner_radius)
            .Rotate(rotation)
            .Resize({ w * 1_pix, h * 1_pix }),
    };

    const cv::Mat& three_channel_image{ loaded_image.GetUnderlying() };
    cv::Mat four_channel_image{};
    cv::cvtColor(three_channel_image, four_channel_image, cv::COLOR_RGB2RGBA);
    const auto encoded_image{ loaded_image.EncodePng() };

    std::lock_guard lock{ m_Mutex };
    m_Cache.push_back({
        std::move(image_path),
        w,
        h,
        rotation,
        std::move(four_channel_image),
    });
    return m_Cache.back().m_PngImage;
}

PngDocument::PngDocument(const Project& project)
    : m_Project{ project }
{
    const auto card_size_with_bleed{ project.CardSizeWithBleed() };
    const dla::ivec2 card_size_pixels{
        static_cast<int32_t>(card_size_with_bleed.x * g_Cfg.m_MaxDPI / 1_pix),
        static_cast<int32_t>(card_size_with_bleed.y * g_Cfg.m_MaxDPI / 1_pix),
    };
    m_PrecomputedCardSize = PixelSize{
        static_cast<float>(card_size_pixels.x) * 1_pix,
        static_cast<float>(card_size_pixels.y) * 1_pix,
    };

    m_PageSize = project.ComputePageSize();
    if (m_Project.m_Data.m_PageSize == Config::c_FitSize)
    {
        const auto page_size_pixels{ card_size_pixels * m_Project.m_Data.m_CardLayoutVertical };
        m_PrecomputedPageSize = PixelSize{
            static_cast<float>(page_size_pixels.x) * 1_pix,
            static_cast<float>(page_size_pixels.y) * 1_pix,
        };
    }
    else
    {
        const dla::ivec2 page_size_pixels{
            static_cast<int32_t>(m_PageSize.x * g_Cfg.m_MaxDPI / 1_pix),
            static_cast<int32_t>(m_PageSize.y * g_Cfg.m_MaxDPI / 1_pix),
        };
        m_PrecomputedPageSize = PixelSize{
            static_cast<float>(page_size_pixels.x) * 1_pix,
            static_cast<float>(page_size_pixels.y) * 1_pix,
        };
    }

    m_ImageCache = std::make_unique<PngImageCache>(project);
}
PngDocument::~PngDocument()
{
}

void PngDocument::ReservePages(size_t pages)
{
    std::lock_guard lock{ m_Mutex };
    m_Pages.reserve(pages);
}

PngPage* PngDocument::NextPage(bool /*is_backside*/)
{
    std::lock_guard lock{ m_Mutex };
    auto& new_page{ m_Pages.emplace_back() };
    new_page.m_Project = &m_Project;
    new_page.m_PageName = std::to_string(m_Pages.size());
    new_page.m_PerfectFit = m_Project.m_Data.m_PageSize == Config::c_FitSize;
    new_page.m_CardSize = m_PrecomputedCardSize;
    new_page.m_PageSize = m_PrecomputedPageSize;
    new_page.m_PageHeight = static_cast<int32_t>(m_PrecomputedPageSize.y / 1_pix);
    new_page.m_Page = cv::Mat::zeros(cv::Size{
                                         static_cast<int32_t>(m_PrecomputedPageSize.x / 1_pix),
                                         static_cast<int32_t>(m_PrecomputedPageSize.y / 1_pix) },
                                     CV_8UC4);
    new_page.m_ImageCache = m_ImageCache.get();
    return &new_page;
}

fs::path PngDocument::Write(fs::path path)
{
    const fs::path png_folder{ fs::path{ path }.replace_extension("") };
    if (!fs::exists(png_folder))
    {
        fs::create_directories(png_folder);
    }
    else if (!fs::is_directory(png_folder))
    {
        fs::remove(png_folder);
        fs::create_directories(png_folder);
    }

    std::lock_guard lock{ m_Mutex };
#if __cpp_lib_ranges_enumerate
    for (const auto& [i, page] : m_Pages | std::views::enumerate)
    {
#else
    for (size_t i = 0; i < m_Pages.size(); i++)
    {
        const PngPage& page{ m_Pages[i] };
#endif

        auto page_name{ page.m_PageName };
        std::ranges::replace(page_name, '/', '_');
        std::ranges::replace(page_name, '.', '_');
        const fs::path png_path{ png_folder / fs::path{ page_name }.replace_extension(".png") };
        {
            const auto png_path_str{ png_path.string() };
            LogInfo("Saving to {}...", png_path_str);
        }
        if (fs::exists(png_path))
        {
            fs::remove(png_path);
        }

        Image{ std::move(page.m_Page) }.Write(png_path, g_Cfg.m_PngCompression.value_or(5), std::nullopt, m_PageSize);
    }

    return png_folder;
}
