#include <ppp/project/image_ops.hpp>

#include <array>
#include <charconv>
#include <ranges>
#include <string>

#include <dla/fmt_format.h>
#include <dla/scalar_math.h>
#include <dla/vector_math.h>

#include <opencv2/opencv.hpp>

#include <QFile>
#include <QString>

#include <ppp/constants.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

#include <ppp/util/log.hpp>

std::vector<fs::path> ListImageFiles(const fs::path& path)
{
    return ListFiles(path, g_ValidImageExtensions);
}

std::vector<fs::path> ListImageFiles(const fs::path& path_one, const fs::path& path_two)
{
    std::vector<fs::path> images{ ListImageFiles(path_one) };
    ForEachFile(
        path_two,
        [&images](const fs::path& path)
        {
            if (!std::ranges::contains(images, path))
            {
                images.push_back(path.filename());
            }
        },
        g_ValidImageExtensions);
    return images;
}

Image CropImage(const Image& image,
                const fs::path& card_name,
                Size card_size,
                Length full_bleed,
                Length bleed_edge,
                PixelDensity max_density)
{
    const Size card_size_with_full_bleed{ card_size + 2 * full_bleed };
    const PixelDensity density{ image.Density(card_size_with_full_bleed) };

    // Safety check: if density is zero or very small, return the original image
    if (density <= 0_dpi)
    {
        LogInfo("Cropping images...\n{} - DPI calculated: 0, skipping cropping for image", card_name.string());
        return image;
    }

    Pixel c{ full_bleed * density };
    {
        const PixelDensity dpi{ (density * 1_in / 1_m) };
        if (bleed_edge > 0_mm)
        {
            const Pixel bleed_pixels = density * bleed_edge;
            c = dla::math::round(dla::math::max(0_pix, c - bleed_pixels));
            LogInfo("Cropping images...\n{} - DPI calculated: {}, cropping {} around frame (adjusted for bleed edge {})",
                    card_name.string(),
                    dpi.value,
                    c,
                    bleed_edge);
        }
        else
        {
            c = dla::math::round(c);
            LogInfo("Cropping images...\n{} - DPI calculated: {}, cropping {} around frame", card_name.string(), dpi.value, c);
        }
    }

    Image cropped_image{ image.Crop(c, c, c, c) };
    if (density > max_density)
    {
        const PixelSize new_size{ dla::round(cropped_image.Size() * (max_density / density)) };
        const PixelDensity max_dpi{ (max_density * 1_in / 1_m) };
        LogInfo("Cropping images...\n{} - Exceeds maximum DPI {}, resizing to {}", card_name.string(), max_dpi.value, static_cast<dla::uvec2>(new_size / 1_pix));
        return cropped_image.Resize(new_size);
    }
    return cropped_image;
}

Image UncropImage(const Image& image, const fs::path& card_name, Size card_size, bool fancy_uncrop)
{
    const PixelDensity density{ image.Density(card_size) };
    Pixel c{ 0.12_in * density };

    const PixelDensity dpi{ (density * 1_in / 1_m) };
    LogInfo("Reinserting bleed edge...\n{} - DPI calculated: {}, adding {} around frame", card_name.string(), dpi.value, c);

    if (fancy_uncrop)
    {
        return image.AddReflectBorder(c, c, c, c);
    }
    else
    {
        return image.AddBlackBorder(c, c, c, c);
    }
}

fs::path GetOutputDir(const fs::path& crop_dir, Length bleed_edge, const std::string& color_cube_name)
{
    if (color_cube_name != "None")
    {
        return GetOutputDir(crop_dir / color_cube_name, bleed_edge, "None");
    }

    const bool has_bleed_edge{ bleed_edge > 0_mm };
    if (has_bleed_edge)
    {
        std::string bleed_folder{ fmt::format("{:.2f}", bleed_edge / 1_mm) };
        std::replace(bleed_folder.begin(), bleed_folder.end(), '.', 'p');
        std::replace(bleed_folder.begin(), bleed_folder.end(), ',', 'p');
        return crop_dir / bleed_folder;
    }

    return crop_dir;
}

ImgDict ReadPreviews(const fs::path& img_cache_file)
{
    try
    {
        if (std::ifstream in_file{ img_cache_file, std::ios_base::binary })
        {
            const auto read = [&in_file]<class T>(TagT<T>) -> T
            {
                T val;
                in_file.read(reinterpret_cast<char*>(&val), sizeof(val));
                return val;
            };
            const auto read_arr = [&in_file, &read]<class T>(TagT<T>) -> std::vector<T>
            {
                const size_t size{ read(c_Tag<size_t>) };
                std::vector<T> buffer(size, T{});

                const size_t data_size{ size * sizeof(T) };
                in_file.read(reinterpret_cast<char*>(buffer.data()), data_size);
                return buffer;
            };

            const size_t version_uint64_read{ read(c_Tag<size_t>) };
            if (version_uint64_read != ImageCacheFormatVersion())
            {
                in_file.close();
                fs::remove(img_cache_file);
                return {};
            }

            const size_t num_images{ read(c_Tag<size_t>) };

            ImgDict img_dict{};
            for (size_t i = 0; i < num_images; ++i)
            {
                const std::vector img_name_buf{ read_arr(c_Tag<char>) };
                const std::string_view img_name{ img_name_buf.data(), img_name_buf.size() };

                ImagePreview img{};

                {
                    const std::vector img_buf{ read_arr(c_Tag<std::byte>) };
                    img.m_CroppedImage = Image::Decode(img_buf);
                }

                {
                    const std::vector img_buf{ read_arr(c_Tag<std::byte>) };
                    img.m_UncroppedImage = Image::Decode(img_buf);
                }

                img.m_BadAspectRatio = read(c_Tag<bool>);

                img_dict[img_name] = std::move(img);
            }

            if (!img_dict.contains(g_Cfg.m_FallbackName))
            {
                ImagePreview img{};
                img.m_CroppedImage = Image::Read(g_Cfg.m_FallbackName);
                img.m_UncroppedImage = img.m_CroppedImage;
                img_dict[g_Cfg.m_FallbackName] = std::move(img);
            }

            return img_dict;
        }
    }
    catch (std::exception& e)
    {
        fmt::print("Failed loading previews: {}", e.what());
        if (fs::exists(img_cache_file))
        {
            fs::remove(img_cache_file);
        }
    }
    return {};
}

void WritePreviews(const fs::path& img_cache_file, const ImgDict& img_dict)
{
    if (std::ofstream out_file{ img_cache_file, std::ios_base::binary | std::ios_base::trunc })
    {
        const auto write = [&out_file](const auto& val)
        {
            out_file.write(reinterpret_cast<const char*>(&val), sizeof(val));
        };
        const auto write_arr = [&out_file, &write](const auto* val, size_t size)
        {
            write(size);

            const size_t data_size{ size * sizeof(*val) };
            out_file.write(reinterpret_cast<const char*>(val), data_size);
        };

        write(ImageCacheFormatVersion());
        write(img_dict.size());
        for (const auto& [name, image] : img_dict)
        {
            const std::string name_str{ name.string() };
            write_arr(name_str.data(), name_str.size());

            {
                const auto buf{ image.m_CroppedImage.EncodeJpg(50) };
                write_arr(buf.data(), buf.size());
            }
            {
                const auto buf{ image.m_UncroppedImage.EncodeJpg(50) };
                write_arr(buf.data(), buf.size());
            }
            write(image.m_BadAspectRatio);
        }
    }
}

cv::Mat LoadColorCube(const fs::path& file_path)
{
    QFile color_cube_file{ ToQString(file_path) };
    color_cube_file.open(QFile::ReadOnly);
    const std::string color_cube_raw{ QLatin1String{ color_cube_file.readAll() }.toString().toStdString() };

    static constexpr auto c_ToStringViews{ std::views::transform(
        [](auto str)
        { return std::string_view(str.data(), str.size()); }) };
    static constexpr auto c_ToInt{ std::views::transform(
        [](std::string_view str)
        {
            int val;
            std::from_chars(str.data(), str.data() + str.size(), val);
            return val;
        }) };
    static constexpr auto c_ToFloat{ std::views::transform(
        [](std::string_view str)
        {
            float val;
#ifdef __clang__
            // Clang and AppleClang do not support std::from_chars overloads with floating points
            val = std::stof(std::string{ str });
#else
            std::from_chars(str.data(), str.data() + str.size(), val);
#endif
            return val;
        }) };
    static constexpr auto c_FloatColorToInt{ std::views::transform(
        [](float val)
        { return static_cast<uint8_t>(val * 255); }) };
    static constexpr auto c_ToColor{ std::views::transform(
        [](std::string_view str)
        {
            return std::views::split(str, ' ') |
                   c_ToStringViews |
                   c_ToFloat |
                   c_FloatColorToInt |
                   std::ranges::to<std::vector>();
        }) };
    const int color_cube_size{
        ((color_cube_raw |
          std::views::split('\n') |
          std::views::drop(4) |
          std::views::take(1) |
          c_ToStringViews |
          std::ranges::to<std::vector>())
             .front() |
         std::views::split(' ') |
         std::views::drop(1) |
         c_ToStringViews |
         c_ToInt |
         std::ranges::to<std::vector>())
            .front()
    };
    const std::vector color_cube_data{
        color_cube_raw |
        std::views::split('\n') |
        std::views::drop(11) |
        c_ToStringViews |
        c_ToColor |
        std::views::join |
        std::ranges::to<std::vector>()
    };

    const int size{ color_cube_size };
    const int data_size{ size * size * size * 3 };

    cv::Mat color_cube;
    color_cube.create(std::vector<int>{ size, size, size }, CV_8UC3);
    color_cube.cols = size;
    color_cube.rows = size;
    assert(color_cube.dataend - color_cube.datastart == data_size);
    memcpy(color_cube.data, color_cube_data.data(), data_size);

    return color_cube;
}
