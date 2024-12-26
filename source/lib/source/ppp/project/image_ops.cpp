#include <ppp/project/image_ops.hpp>

#include <array>
#include <ranges>

#include <dla/fmt_format.h>
#include <dla/scalar_math.h>
#include <dla/vector_math.h>

#include <opencv2/opencv.hpp>

#include <QFile>
#include <QString>

#include <ppp/constants.hpp>

#include <ppp/version.hpp>

void InitFolders(const fs::path& image_dir, const fs::path& crop_dir)
{
    for (const auto& folder : { image_dir, crop_dir })
    {
        if (!std::filesystem::exists(folder))
        {
            std::filesystem::create_directories(folder);
        }
    }
}

template<class FunT>
void ForEachImageFile(const fs::path& path, FunT&& fun)
{
    ForEachFile(path, std::forward<FunT>(fun), ValidImageExtensions);
}

std::vector<fs::path> ListImageFiles(const fs::path& path)
{
    return ListFiles(path, ValidImageExtensions);
}

Image CropImage(const Image& image, const fs::path& image_name, Length bleed_edge, PixelDensity max_density, PrintFn print_fn)
{
    const PixelDensity density{ image.Density(CardSizeWithBleed) };
    Pixel c{ 0.12_in * density };
    {
        const PixelDensity dpi{ (density * 1_in / 1_m) };
        if (bleed_edge > 0_mm)
        {
            const Pixel bleed_pixels = density * bleed_edge;
            c = dla::math::round(c - bleed_pixels);
            PPP_LOG("Cropping images...\n{} - DPI calculated: {}, cropping {} around frame (adjusted for bleed edge {})",
                    image_name.string(),
                    dpi.value,
                    c,
                    bleed_edge);
        }
        else
        {
            c = dla::math::round(c);
            PPP_LOG("Cropping images...\n{} - DPI calculated: {}, cropping {} around frame", image_name.string(), dpi.value, c);
        }
    }

    Image cropped_image{ image.Crop(c, c, c, c) };
    if (density > max_density)
    {
        const PixelSize new_size{ dla::round(cropped_image.Size() * (max_density / density)) };
        const PixelDensity max_dpi{ (max_density * 1_in / 1_m) };
        PPP_LOG("Cropping images...\n{} - Exceeds maximum DPI {}, resizing to {}", max_dpi.value, image_name.string(), new_size);
        return cropped_image.Resize(new_size);
    }
    return cropped_image;
}

Image UncropImage(const Image& image, const fs::path& image_name, PrintFn print_fn)
{
    const PixelDensity density{ image.Density(CardSizeWithoutBleed) };
    Pixel c{ 0.12_in * density };

    const PixelDensity dpi{ (density * 1_in / 1_m) };
    PPP_LOG("Reinserting bleed edge...\n{} - DPI calculated: {}, adding {} around frame", image_name.string(), dpi.value, c);

    return image.AddBlackBorder(c, c, c, c);
}

fs::path GetOutputDir(const fs::path& crop_dir, Length bleed_edge, bool do_vibrance_bump)
{
    const bool has_bleed_edge{ bleed_edge > 0_mm };

    if (do_vibrance_bump)
    {
        return GetOutputDir(crop_dir / "vibrance", bleed_edge, false);
    }

    if (has_bleed_edge)
    {
        std::string bleed_folder{ std::to_string(bleed_edge.value * 1000) };
        bleed_folder.erase(bleed_folder.find_last_not_of("0.") + 1);
        std::replace(bleed_folder.begin(), bleed_folder.end(), '.', 'p');
        return crop_dir / bleed_folder;
    }

    return crop_dir;
}

bool NeedRunCropper(const fs::path& image_dir, const fs::path& crop_dir, Length bleed_edge, bool do_vibrance_bump)
{
    const fs::path output_dir{ GetOutputDir(crop_dir, bleed_edge, do_vibrance_bump) };
    if (!fs::exists(output_dir))
    {
        return true;
    }

    std::vector input_files{ ListImageFiles(image_dir) };
    std::ranges::sort(input_files);
    std::vector output_files{ ListImageFiles(output_dir) };
    std::ranges::sort(output_files);
    return input_files != output_files;
}

ImgDict RunCropper(const fs::path& image_dir,
                   const fs::path& crop_dir,
                   const fs::path& img_cache_file,
                   const ImgDict& img_dict,
                   Length bleed_edge,
                   PixelDensity max_density,
                   const cv::Mat* vibrance_cube,
                   bool uncrop,
                   PrintFn print_fn)
{
    ImgDict out_img_dict{ img_dict };

    const bool has_bleed_edge{ bleed_edge > 0_mm };
    const bool do_vibrance_bump{ vibrance_cube != nullptr };
    if (has_bleed_edge)
    {
        // Do base cropping, always needed
        out_img_dict = RunCropper(image_dir, crop_dir, img_cache_file, out_img_dict, 0_mm, max_density, vibrance_cube, uncrop, print_fn);
    }
    else if (do_vibrance_bump)
    {
        // Do base cropping without vibrance for previews
        out_img_dict = RunCropper(image_dir, crop_dir, img_cache_file, out_img_dict, 0_mm, max_density, nullptr, uncrop, print_fn);
    }

    const fs::path output_dir{ GetOutputDir(crop_dir, bleed_edge, do_vibrance_bump) };
    if (!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }

    const std::vector input_files{ ListImageFiles(image_dir) };
    for (const auto& img_file : input_files)
    {
        if (fs::exists(output_dir / img_file))
        {
            continue;
        }

        const Image image{ Image::Read(image_dir / img_file) };
        const Image cropped_image{ CropImage(image, img_file, bleed_edge, max_density, print_fn) };
        if (do_vibrance_bump)
        {
            const Image vibrant_image{ cropped_image.ApplyColorCube(*vibrance_cube) };
            vibrant_image.Write(output_dir / img_file);
        }
        else
        {
            cropped_image.Write(output_dir / img_file);
        }
    }

    std::vector<fs::path> extra_files{};

    const std::vector output_files{ ListImageFiles(output_dir) };
    for (const auto& img_file : output_files)
    {
        if (!fs::exists(image_dir / img_file))
        {
            extra_files.push_back(img_file);
        }
    }

    if (uncrop && !has_bleed_edge)
    {
        for (const auto& extra_img : extra_files)
        {
            const Image image{ Image::Read(output_dir / extra_img) };
            const Image uncropped_image{ UncropImage(image, extra_img, print_fn) };
            uncropped_image.Write(image_dir / extra_img);
        }
    }
    else
    {
        for (const auto& extra_img : extra_files)
        {
            fs::remove(output_dir / extra_img);
        }
    }

    if (NeedCachePreviews(crop_dir, out_img_dict))
    {
        out_img_dict = CachePreviews(image_dir, crop_dir, img_cache_file, img_dict, print_fn);
    }

    return out_img_dict;
}

bool NeedCachePreviews(const fs::path& crop_dir, const ImgDict& img_dict)
{
    const std::vector crop_list{ ListImageFiles(crop_dir) };

    for (const auto& img : crop_list)
    {
        if (!img_dict.contains(img) || img_dict.at(img).UncroppedImage.Width() != CFG.BasePreviewWidth)
        {
            return true;
        }
    }

    for (const auto& [img, _] : img_dict)
    {
        if (img == "fallback.png")
        {
            continue;
        }

        if (!std::ranges::contains(crop_list, img))
        {
            return true;
        }
    }

    return false;
}

ImgDict CachePreviews(const fs::path& image_dir, const fs::path& crop_dir, const fs::path& img_cache_file, const ImgDict& img_dict, PrintFn print_fn)
{
    ImgDict out_img_dict{};
    for (const auto& [img, _] : img_dict)
    {
        if (fs::exists(crop_dir / img) && img_dict.contains(img) && img_dict.at(img).UncroppedImage.Width() == CFG.BasePreviewWidth)
        {
            out_img_dict[img] = img_dict.at(img);
        }
    }

    const fs::path fallback_img{ "fallback.png" };
    if (img_dict.contains(fallback_img) && img_dict.at(fallback_img).UncroppedImage.Width() == CFG.BasePreviewWidth)
    {
        out_img_dict[fallback_img] = img_dict.at(fallback_img);
    }

    const PixelSize uncropped_size{ CFG.BasePreviewWidth, dla::math::round(CFG.BasePreviewWidth / CardRatio) };
    const PixelSize thumb_size{ uncropped_size / 2.0f };

    {
        const bool has_img{ out_img_dict.contains(fallback_img) };
        if (!has_img)
        {
            PPP_LOG("Caching fallback image {}...", fallback_img.string());
            ImagePreview& image_preview{ out_img_dict[fallback_img] };
            image_preview.UncroppedImage = Image::Read(fallback_img);
            image_preview.CroppedImage = CropImage(image_preview.UncroppedImage, fallback_img, 0_mm, 1200_dpi, nullptr);
            image_preview.CroppedThumbImage = image_preview.CroppedImage.Resize(thumb_size);
        }
    }

    const std::vector input_files{ ListImageFiles(image_dir) };
    for (const auto& img : input_files)
    {
        const bool has_img{ out_img_dict.contains(img) };
        if (has_img)
        {
            continue;
        }

        ImagePreview& image_preview{ out_img_dict[img] };

        if (fs::exists(image_dir / img))
        {
            const Image image{ Image::Read(image_dir / img) };

            PPP_LOG("Caching uncropped preview for image {}...", img.string());
            image_preview.UncroppedImage = image.Resize(uncropped_size);

            PPP_LOG("Caching cropped preview for image {}...", img.string());
            image_preview.CroppedImage = CropImage(image_preview.UncroppedImage, img, 0_mm, 1200_dpi, nullptr);

            PPP_LOG("Caching cropped preview for image {}...", img.string());
            image_preview.CroppedThumbImage = image_preview.CroppedImage.Resize(thumb_size);
        }
        else
        {
            PPP_LOG("Failed caching uncropped preview for image {}...", img.string());
            image_preview = out_img_dict[fallback_img];
        }
    }

    WritePreviews(img_cache_file, out_img_dict);

    return out_img_dict;
}

ImgDict ReadPreviews(const fs::path& img_cache_file)
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
            const size_t size{ read(Tag<size_t>) };
            std::vector<T> buffer(size, T{});

            const size_t data_size{ size * sizeof(T) };
            in_file.read(reinterpret_cast<char*>(buffer.data()), data_size);
            return buffer;
        };

        const size_t version_uint64_read{ read(Tag<size_t>) };
        if (version_uint64_read != ImageCacheFormatVersion())
        {
            in_file.close();
            fs::remove(img_cache_file);
            return {};
        }

        const size_t num_images{ read(Tag<size_t>) };

        ImgDict img_dict{};
        for (size_t i = 0; i < num_images; ++i)
        {
            const std::vector img_name_buf{ read_arr(Tag<char>) };
            const std::string_view img_name{ img_name_buf.data(), img_name_buf.size() };

            ImagePreview img{};

            {
                const std::vector img_buf{ read_arr(Tag<std::byte>) };
                img.CroppedImage = Image::Decode(img_buf);
            }

            {
                const std::vector img_buf{ read_arr(Tag<std::byte>) };
                img.UncroppedImage = Image::Decode(img_buf);
            }

            {
                const std::vector img_buf{ read_arr(Tag<std::byte>) };
                img.CroppedThumbImage = Image::Decode(img_buf);
            }

            img_dict[img_name] = std::move(img);
        }
        return img_dict;
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
                const std::vector buf{ image.CroppedImage.Encode() };
                write_arr(buf.data(), buf.size());
            }
            {
                const std::vector buf{ image.UncroppedImage.Encode() };
                write_arr(buf.data(), buf.size());
            }
            {
                const std::vector buf{ image.CroppedThumbImage.Encode() };
                write_arr(buf.data(), buf.size());
            }
        }
    }
}

cv::Mat LoadColorCube(const fs::path& file_path)
{
    QFile color_cube_file{ QString::fromWCharArray(file_path.c_str()) };
    color_cube_file.open(QFile::ReadOnly);
    const std::string color_cube_raw{ QLatin1String{ color_cube_file.readAll() }.toString().toStdString() };

    static constexpr auto to_string_views{ std::views::transform(
        [](auto str)
        { return std::string_view(str.data(), str.size()); }) };
    static constexpr auto to_int{ std::views::transform(
        [](std::string_view str)
        {
            int val;
            std::from_chars(str.data(), str.data() + str.size(), val);
            return val;
        }) };
    static constexpr auto to_float{ std::views::transform(
        [](std::string_view str)
        {
            float val;
            std::from_chars(str.data(), str.data() + str.size(), val);
            return val;
        }) };
    static constexpr auto float_color_to_byte{ std::views::transform(
        [](float val)
        { return static_cast<uint8_t>(val * 255); }) };
    static constexpr auto to_color{ std::views::transform(
        [](std::string_view str)
        {
            return std::views::split(str, ' ') |
                   to_string_views |
                   to_float |
                   float_color_to_byte |
                   std::ranges::to<std::vector>();
        }) };
    const int color_cube_size{
        ((color_cube_raw |
          std::views::split('\n') |
          std::views::drop(4) |
          std::views::take(1) |
          to_string_views |
          std::ranges::to<std::vector>())
             .front() |
         std::views::split(' ') |
         std::views::drop(1) |
         to_string_views |
         to_int |
         std::ranges::to<std::vector>())
            .front()
    };
    const std::vector color_cube_data{
        color_cube_raw |
        std::views::split('\n') |
        std::views::drop(11) |
        to_string_views |
        to_color |
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
