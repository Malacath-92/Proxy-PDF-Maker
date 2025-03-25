#include <ppp/pdf/png_backend.hpp>

#include <crc32c/crc32c.h>

#include <opencv2/imgproc.hpp>

#include <ppp/project/project.hpp>

inline int32_t ToPixels(Length l)
{
    return static_cast<int32_t>(std::ceil(l * CFG.MaxDPI / 1_pix));
}

void PngPage::DrawDashedLine(std::array<ColorRGB32f, 2> colors, Length fx, Length fy, Length tx, Length ty)
{
    const auto real_fx{ ToPixels(fx) };
    const auto real_fy{ ToPixels(fy) };
    const auto real_tx{ ToPixels(tx) };
    const auto real_ty{ ToPixels(ty) };
    const auto real_w{ (ToPixels(0.2_mm) / 2) * 2 };

    const cv::Point line_from{ real_fx, real_fy };
    const cv::Point line_to{ real_tx, real_ty };
    const cv::Point delta{ line_to - line_from };

    const cv::Point perp{ delta.x == 0 ? cv::Point{ real_w / 2, 0 } : cv::Point{ 0, real_w / 2 } };
    const cv::Point from{ line_from - perp };
    const cv::Point to{ line_to + perp };

    const cv::Scalar color_a{ colors[0].b * 255, colors[0].g * 255, colors[0].r * 255, 255.0f };
    const cv::Scalar color_b{ colors[1].b * 255, colors[1].g * 255, colors[1].r * 255, 255.0f };

    cv::rectangle(Page, from, to, color_a, cv::FILLED);

    constexpr float dash_freq{ 0.25f };
    for (float alpha = 0; alpha < 1.0f; alpha += dash_freq)
    {
        const cv::Point sub_from{ from + alpha * delta };
        const cv::Point sub_to{ from + 2 * perp + (alpha + dash_freq / 2.0f) * delta };
        cv::rectangle(Page, sub_from, sub_to, color_b, cv::FILLED);
    }
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
        const auto page_size_pixels{ card_size_pixels * TheProject.CustomCardLayout };
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

        std::vector<uchar> buf;
        if (cv::imencode(".png", page.Page, buf, png_params))
        {
            // Squeeze in a pHYs chunk that contains dpi
            {
                // Find first IDAT section ...
                size_t idat_idx{};
                for (size_t j = 0; j < buf.size() - 4; j++)
                {
                    if (std::string_view{ reinterpret_cast<std::string_view::value_type*>(&buf[j]), 4 } == "IDAT")
                    {
                        idat_idx = j - 4;
                        break;
                    }
                }

                auto reverse_endianness{
                    [](uint32_t val) -> uint32_t
                    {
                        union
                        {
                            uint32_t v;
                            unsigned char u8[sizeof(uint32_t)];
                        } util;
                        util.v = val;
                        std::swap(util.u8[0], util.u8[3]);
                        std::swap(util.u8[1], util.u8[2]);
                        return util.v;
                    }
                };

                // Create pHYs chunk...
                struct
                {
                    uint32_t size;
                    std::array<char, 4> name;
                    uint32_t dots_per_meter_x;
                    uint32_t dots_per_meter_y;
                    uint8_t unit;
                    uint8_t pad[3];
                } const pHYs_chunk{
                    reverse_endianness(9),
                    { 'p', 'H', 'Y', 's' },
                    reverse_endianness(static_cast<uint32_t>(CFG.MaxDPI.value)),
                    reverse_endianness(static_cast<uint32_t>(CFG.MaxDPI.value)),
                    1, // this just means meter
                };

                // size + name + data + padding
                static constexpr uint32_t pHYs_chunk_size{ 4 + 4 + 9 };
                // chunk + padding
                static_assert(sizeof(pHYs_chunk) == pHYs_chunk_size + 3);
                // verify padding is at the end ...
                static_assert(offsetof(decltype(pHYs_chunk), unit) == pHYs_chunk_size - 1);

                // only the name and data
                const auto* crc_data{ reinterpret_cast<const uchar*>(&pHYs_chunk) + 4 };
                const auto crc_data_size{ pHYs_chunk_size - 4 };
                const uint32_t crc{ reverse_endianness(crc32c::Crc32c(crc_data, crc_data_size)) };

                // chunk + crc
                std::array<uchar, pHYs_chunk_size + 4> pHYs_buf;
                std::memcpy(pHYs_buf.data(), &pHYs_chunk, pHYs_chunk_size);
                std::memcpy(pHYs_buf.data() + pHYs_chunk_size, &crc, 4);
                buf.insert(buf.begin() + idat_idx, pHYs_buf.begin(), pHYs_buf.end());
            }

            if (FILE * file{ fopen(png_path.string().c_str(), "wb") })
            {
                fwrite(buf.data(), 1, buf.size(), file);
                fclose(file);
            }
        }
    }

    return png_folder;
}
