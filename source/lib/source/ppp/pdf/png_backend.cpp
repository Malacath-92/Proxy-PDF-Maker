#include <ppp/pdf/png_backend.hpp>

#include <opencv2/imgproc.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

inline int32_t ToPixels(Length l)
{
    return static_cast<int32_t>(std::ceil(l * CFG.MaxDPI / 1_pix));
}

void PngPage::DrawSolidLine(LineData data, LineStyle style)
{
    const auto& fx{ data.m_From.x };
    const auto& fy{ data.m_From.y };
    const auto& tx{ data.m_To.x };
    const auto& ty{ data.m_To.y };

    const auto real_fx{ ToPixels(fx) };
    const auto real_fy{ ToPixels(fy) };
    const auto real_tx{ ToPixels(tx) };
    const auto real_ty{ ToPixels(ty) };
    const auto real_w{ (ToPixels(style.m_Thickness) / 2) * 2 };

    const cv::Point line_from{ real_fx, real_fy };
    const cv::Point line_to{ real_tx, real_ty };
    const cv::Point delta{ line_to - line_from };

    const cv::Point perp{ delta.x == 0 ? cv::Point{ real_w / 2, 0 } : cv::Point{ 0, real_w / 2 } };
    const cv::Point from{ line_from - perp };
    const cv::Point to{ line_to + perp };

    const cv::Scalar color_cv{ style.m_Color.b * 255, style.m_Color.g * 255, style.m_Color.r * 255, 255.0f };

    cv::rectangle(m_Page, from, to, color_cv, cv::FILLED);
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
    const auto real_fy{ ToPixels(fy) };
    const auto real_tx{ ToPixels(tx) };
    const auto real_ty{ ToPixels(ty) };
    const auto real_w{ (ToPixels(style.m_Thickness) / 2) * 2 };

    const cv::Point line_from{ real_fx, real_fy };
    const cv::Point line_to{ real_tx, real_ty };
    const cv::Point delta{ line_to - line_from };

    const cv::Point perp{ delta.x == 0 ? cv::Point{ real_w / 2, 0 } : cv::Point{ 0, real_w / 2 } };
    const cv::Point from{ line_from - perp };
    const cv::Point to{ line_to + perp };

    const cv::Scalar color_a{ style.m_Color.b * 255, style.m_Color.g * 255, style.m_Color.r * 255, 255.0f };
    const cv::Scalar color_b{ style.m_SecondColor.b * 255, style.m_SecondColor.g * 255, style.m_SecondColor.r * 255, 255.0f };

    cv::rectangle(m_Page, from, to, color_a, cv::FILLED);

    const float dash_freq{ style.m_DashSize / dla::distance(data.m_From, data.m_To) };
    for (size_t i = 0; i < static_cast<size_t>(0.5f / dash_freq); ++i)
    {
        const float alpha{ i * dash_freq * 2.0f };
        const cv::Point sub_from{ from + alpha * delta };
        const cv::Point sub_to{ from + 2 * perp + (alpha + dash_freq / 2.0f) * delta };
        cv::rectangle(m_Page, sub_from, sub_to, color_b, cv::FILLED);
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
        const auto real_x{ card_idx_x * static_cast<int32_t>(m_CardSize.x / 1_pix) };
        const auto real_y{ card_idx_y * static_cast<int32_t>(m_CardSize.y / 1_pix) };
        const auto real_w{ static_cast<int32_t>(m_CardSize.x / 1_pix) };
        const auto real_h{ static_cast<int32_t>(m_CardSize.y / 1_pix) };
        m_ImageCache->GetImage(image_path, real_w, real_h, rotation)
            .copyTo(m_Page(cv::Rect(real_x, real_y, real_w, real_h)));
    }
    else
    {
        const auto& w{ data.m_Size.x };
        const auto& h{ data.m_Size.y };

        const auto real_x{ ToPixels(x) };
        const auto real_y{ ToPixels(y) };
        const auto real_w{ ToPixels(w) };
        const auto real_h{ ToPixels(h) };
        m_ImageCache->GetImage(image_path, real_w, real_h, rotation)
            .copyTo(m_Page(cv::Rect(real_x, real_y, real_w, real_h)));
    }
}

void PngPage::DrawText(std::string_view text, TextBoundingBox bounding_box)
{
    const auto real_left{ ToPixels(bounding_box.m_TopLeft.x) };
    const auto real_top{ ToPixels(bounding_box.m_TopLeft.x) };
    const cv::Scalar black{ 0, 0, 0, 255.0f };

    cv::putText(m_Page, cv::String{ text.data(), text.size() }, cv::Point{ real_left, real_top }, cv::FONT_HERSHEY_PLAIN, 12, black);
}

const cv::Mat& PngImageCache::GetImage(fs::path image_path, int32_t w, int32_t h, Image::Rotation rotation)
{
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

    const Image loaded_image{
        Image::Read(image_path)
            .Rotate(rotation)
            .Resize({ w * 1_pix, h * 1_pix }),
    };
    const cv::Mat& three_channel_image{ loaded_image.GetUnderlying() };
    cv::Mat four_channel_image{};
    cv::cvtColor(three_channel_image, four_channel_image, cv::COLOR_RGB2RGBA);
    const auto encoded_image{ loaded_image.EncodePng() };
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
        static_cast<int32_t>(card_size_with_bleed.x * CFG.MaxDPI / 1_pix),
        static_cast<int32_t>(card_size_with_bleed.y * CFG.MaxDPI / 1_pix),
    };
    m_PrecomputedCardSize = PixelSize{
        static_cast<float>(card_size_pixels.x) * 1_pix,
        static_cast<float>(card_size_pixels.y) * 1_pix,
    };

    const auto page_size_pixels{ card_size_pixels * m_Project.Data.CardLayout };
    m_PrecomputedPageSize = PixelSize{
        static_cast<float>(page_size_pixels.x) * 1_pix,
        static_cast<float>(page_size_pixels.y) * 1_pix,
    };

    m_PageSize = project.ComputePageSize();

    m_ImageCache = std::make_unique<PngImageCache>();
}
PngDocument::~PngDocument()
{
}

PngPage* PngDocument::NextPage()
{
    auto& new_page{ m_Pages.emplace_back() };
    new_page.m_Project = &m_Project;
    new_page.m_Document = this;
    new_page.m_PerfectFit = m_Project.Data.PageSize == Config::FitSize;
    new_page.m_CardSize = m_PrecomputedCardSize;
    new_page.m_PageSize = m_PrecomputedPageSize;
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

#if __cpp_lib_ranges_enumerate
    for (const auto& [i, page] : m_Pages | std::views::enumerate)
    {
#else
    for (size_t i = 0; i < m_Pages.size(); i++)
    {
        const PngPage& page{ m_Pages[i] };
#endif

        const fs::path png_path{ png_folder / fs::path{ std::to_string(i) }.replace_extension(".png") };
        {
            const auto png_path_str{ png_path.string() };
            LogInfo("Saving to {}...", png_path_str);
        }
        if (fs::exists(png_path))
        {
            fs::remove(png_path);
        }

        Image{ std::move(page.m_Page) }.Write(png_path, CFG.PngCompression.value_or(5), std::nullopt, m_PageSize);
    }

    return png_folder;
}
