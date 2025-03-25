#include <ppp/pdf/png_backend.hpp>

#include <opencv2/imgproc.hpp>

#include <ppp/project/project.hpp>

inline int ToPixels(Length l)
{
    return static_cast<int>(std::ceil(l * CFG.MaxDPI / 1_pix));
}

void PngPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    (void)colors;
    (void)fx;
    (void)fy;
    (void)tx;
    (void)ty;

    // const auto real_fx{ ToPixels(fx) };
    // const auto real_fy{ ToPixels(fy) };
    // const auto real_tx{ ToPixels(tx) };
    // const auto real_ty{ ToPixels(ty) };

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

void PngPage::DrawDashedCross(std::array<ColorRGB32f, 2> colors, Length x, Length y, CrossSegment s)
{
    const auto [dx, dy]{ CrossSegmentOffsets[static_cast<size_t>(s)].pod() };
    const auto tx{ x + 3_mm * dx };
    const auto ty{ y + 3_mm * dy };

    DrawDashedLine(colors, x, y, tx, y);
    DrawDashedLine(colors, x, y, x, ty);
}

void PngPage::DrawImage(const fs::path& image_path, Length x, Length y, Length w, Length h, Image::Rotation rotation)
{
    if (PerfectFit)
    {
        const auto card_idx_x{ static_cast<int32_t>(std::round(static_cast<float>(static_cast<float>(ToPixels(x)) / CardSize.x))) };
        const auto card_idx_y{ static_cast<int32_t>(std::round(static_cast<float>(static_cast<float>(ToPixels(y)) / CardSize.y))) };
        const auto real_x{ card_idx_x * static_cast<int32_t>(CardSize.x / 1_pix) };
        const auto real_y{ card_idx_y * static_cast<int32_t>(CardSize.y / 1_pix) };
        const auto real_w{ static_cast<int32_t>(CardSize.x / 1_pix) };
        const auto real_h{ static_cast<int32_t>(CardSize.y / 1_pix) };
        ImageCache->GetImage(image_path, w, h, rotation).copyTo(Page(cv::Rect(real_x, real_y, real_w, real_h)));
    }
    else
    {
        const auto real_x{ ToPixels(x) };
        const auto real_y{ ToPixels(y) };
        const auto real_w{ ToPixels(w) };
        const auto real_h{ ToPixels(h) };
        ImageCache->GetImage(image_path, w, h, rotation).copyTo(Page(cv::Rect(real_x, real_y, real_w, real_h)));
    }
}

const cv::Mat& PngImageCache::GetImage(fs::path image_path, Length w, Length h, Image::Rotation rotation)
{
    const auto it{
        std::ranges::find_if(image_cache, [&](const ImageCacheEntry& entry)
                             { return entry.ImageRotation == rotation && entry.Width == w && entry.Height == h && entry.ImagePath == image_path; })
    };
    if (it != image_cache.end())
    {
        return it->PngImage;
    }

    const auto real_w{ w * CFG.MaxDPI };
    const auto real_h{ h * CFG.MaxDPI };

    const Image loaded_image{
        Image::Read(image_path)
            .Rotate(rotation)
            .Resize({ real_w, real_h }),
    };
    const cv::Mat& three_channel_image{ loaded_image.GetUnderlying() };
    cv::Mat four_channel_image{};
    cv::cvtColor(three_channel_image, four_channel_image, cv::COLOR_RGB2RGBA);
    const auto encoded_image{ loaded_image.Encode() };
    image_cache.push_back({
        std::move(image_path),
        w,
        h,
        rotation,
        std::move(four_channel_image),
    });
    return image_cache.back().PngImage;
}

PngDocument::PngDocument(const Project& project, PrintFn print_fn)
    : TheProject{ project }
    , PrintFunction{ std::move(print_fn) }
{
    const auto card_size_with_bleed{ CardSizeWithoutBleed + 2 * TheProject.BleedEdge };
    const dla::ivec2 card_size_pixels{
        static_cast<int32_t>(card_size_with_bleed.x * CFG.MaxDPI / 1_pix),
        static_cast<int32_t>(card_size_with_bleed.y * CFG.MaxDPI / 1_pix),
    };
    PrecomputedCardSize = PixelSize{
        static_cast<float>(card_size_pixels.x) * 1_pix,
        static_cast<float>(card_size_pixels.y) * 1_pix,
    };

    if (TheProject.PageSize == "Fit")
    {
        const auto page_size_pixels{ card_size_pixels * static_cast<dla::ivec2>(TheProject.CustomCardLayout) };
        PrecomputedPageSize = PixelSize{
            static_cast<float>(page_size_pixels.x) * 1_pix,
            static_cast<float>(page_size_pixels.y) * 1_pix,
        };
    }
    else
    {
        const auto page_size{ CFG.PageSizes[project.PageSize].Dimensions };
        const cv::Size page_size_pixels{ ToPixels(page_size.x), ToPixels(page_size.y) };
        PrecomputedPageSize = PixelSize{
            static_cast<float>(page_size_pixels.width) * 1_pix,
            static_cast<float>(page_size_pixels.height) * 1_pix,
        };
    }

    ImageCache = std::make_unique<PngImageCache>();
}
PngDocument::~PngDocument()
{
}

PngPage* PngDocument::NextPage(Size /*page_size*/)
{
    auto& new_page{ Pages.emplace_back() };
    new_page.TheProject = &TheProject;
    new_page.PerfectFit = TheProject.PageSize == "Fit";
    new_page.CardSize = PrecomputedCardSize;
    new_page.PageSize = PrecomputedPageSize;
    new_page.Page = cv::Mat::zeros(cv::Size{ static_cast<int32_t>(PrecomputedPageSize.x / 1_pix), static_cast<int32_t>(PrecomputedPageSize.y / 1_pix) }, CV_8UC4);
    new_page.ImageCache = ImageCache.get();
    return &new_page;
}

std::optional<fs::path> PngDocument::Write(fs::path path)
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

    std::vector<int> png_params{
        cv::IMWRITE_PNG_COMPRESSION,
        CFG.PngCompression.value_or(5),
        cv::IMWRITE_PNG_STRATEGY,
        cv::IMWRITE_PNG_STRATEGY_DEFAULT,
    };

    for (const auto& [i, page] : std::ranges::enumerate_view(Pages))
    {
        const fs::path png_path{ png_folder / fs::path{ std::to_string(i) }.replace_extension(".png") };
        {
            const auto png_path_str{ png_path.string() };
            PPP_LOG_WITH(PrintFunction, "Saving to {}...", png_path_str);
        }
        if (fs::exists(png_path))
        {
            fs::remove(png_path);
        }
        cv::imwrite(png_path.string(), page.Page, png_params);
    }

    return png_folder;
}
