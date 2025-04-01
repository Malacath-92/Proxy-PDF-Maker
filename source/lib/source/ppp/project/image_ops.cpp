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
#include <ppp/qt_util.hpp>
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

std::vector<fs::path> ListImageFiles(const fs::path& path)
{
    return ListFiles(path, ValidImageExtensions);
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
        ValidImageExtensions);
    return images;
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
        PPP_LOG("Cropping images...\n{} - Exceeds maximum DPI {}, resizing to {}", image_name.string(), max_dpi.value, static_cast<dla::uvec2>(new_size / 1_pix));
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

fs::path GetOutputDir(const fs::path& crop_dir, Length bleed_edge, const std::string& color_cube_name)
{
    const bool has_bleed_edge{ bleed_edge > 0_mm };

    if (color_cube_name != "None")
    {
        return GetOutputDir(crop_dir / color_cube_name, bleed_edge, "None");
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

bool NeedRunMinimalCropper(const fs::path& image_dir,
                           const fs::path& crop_dir,
                           std::span<const fs::path> card_list,
                           Length bleed_edge,
                           const std::string& color_cube_name)
{
    const fs::path output_dir{ GetOutputDir(crop_dir, bleed_edge, color_cube_name) };
    if (!fs::exists(output_dir))
    {
        return true;
    }

    std::vector output_files{ ListImageFiles(output_dir) };

    {
        // Do we need to run any input -> output operation?
        std::vector input_files{ ListImageFiles(image_dir) };
        for (const auto& img_file : input_files)
        {
            if (!std::ranges::contains(output_files, img_file) && std::ranges::contains(card_list, img_file))
            {
                return true;
            }
        }
    }

    if (color_cube_name != "None")
    {
        const fs::path intermediate_dir{ GetOutputDir(crop_dir, bleed_edge, "None") };
        std::vector intermediate_files{ ListImageFiles(intermediate_dir) };

        // Do we need to run any intermediate -> output operation?
        for (const auto& img_file : intermediate_files)
        {
            if (!std::ranges::contains(output_files, img_file) && std::ranges::contains(card_list, img_file))
            {
                return true;
            }
        }
    }

    if (CFG.EnableUncrop && bleed_edge > 0_mm)
    {
        // Do we need to run any crop -> input -> output operation?
        std::vector crop_files{ ListImageFiles(crop_dir) };
        for (const auto& img_file : crop_files)
        {
            if (!std::ranges::contains(output_files, img_file) && std::ranges::contains(card_list, img_file))
            {
                return true;
            }
        }
    }

    return false;
}

void RunMinimalCropper(const fs::path& image_dir,
                       const fs::path& crop_dir,
                       std::span<const fs::path> card_list,
                       Length bleed_edge,
                       PixelDensity max_density,
                       const std::string& color_cube_name,
                       const cv::Mat* color_cube,
                       PrintFn print_fn)
{
    const fs::path output_dir{ GetOutputDir(crop_dir, bleed_edge, color_cube_name) };
    if (!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }

    if (CFG.EnableUncrop && bleed_edge > 0_mm)
    {
        const fs::path intermediate_dir{ GetOutputDir(crop_dir, bleed_edge, "None") };
        if (!fs::exists(intermediate_dir))
        {
            fs::create_directories(intermediate_dir);
        }

        for (const auto& img_file : card_list)
        {
            if (fs::exists(output_dir / img_file))
            {
                continue;
            }

            if (fs::exists(intermediate_dir / img_file))
            {
                continue;
            }

            if (!fs::exists(crop_dir / img_file))
            {
                continue;
            }

            const Image image{ UncropImage(Image::Read(crop_dir / img_file), img_file, print_fn) };
            const Image cropped_image{ CropImage(image, img_file, bleed_edge, max_density, print_fn) };
            cropped_image.Write(intermediate_dir / img_file);
        }
    }

    const bool do_vibrance_bump{ color_cube != nullptr };
    if (do_vibrance_bump)
    {
        const fs::path intermediate_dir{ GetOutputDir(crop_dir, bleed_edge, "None") };
        if (!fs::exists(intermediate_dir))
        {
            fs::create_directories(intermediate_dir);
        }

        for (const auto& img_file : card_list)
        {
            if (fs::exists(output_dir / img_file))
            {
                continue;
            }

            if (fs::exists(intermediate_dir / img_file))
            {
                continue;
            }

            const Image image{ Image::Read(image_dir / img_file) };
            const Image cropped_image{ CropImage(image, img_file, bleed_edge, max_density, print_fn) };
            cropped_image.Write(intermediate_dir / img_file);

            const Image vibrant_image{ Image::Read(intermediate_dir / img_file).ApplyColorCube(*color_cube) };
            vibrant_image.Write(output_dir / img_file);
        }

        for (const auto& img_file : card_list)
        {
            if (fs::exists(output_dir / img_file))
            {
                continue;
            }

            const Image vibrant_image{ Image::Read(intermediate_dir / img_file).ApplyColorCube(*color_cube) };
            vibrant_image.Write(output_dir / img_file);
        }
    }
    else
    {
        for (const auto& img_file : card_list)
        {
            if (fs::exists(output_dir / img_file))
            {
                continue;
            }

            const Image image{ Image::Read(image_dir / img_file) };
            const Image cropped_image{ CropImage(image, img_file, bleed_edge, max_density, print_fn) };
            cropped_image.Write(output_dir / img_file);
        }
    }
}

bool NeedCachePreviews(const fs::path& crop_dir, const ImgDict& img_dict)
{
    const std::vector crop_list{ ListImageFiles(crop_dir) };

    for (const auto& img : crop_list)
    {
        // Image not in previews
        if (!img_dict.contains(img))
        {
            return true;
        }

        const auto& preview{ img_dict.at(img) };

        // Image in previews has wrong size
        if (preview.UncroppedImage.Width() != CFG.BasePreviewWidth)
        {
            return true;
        }
    }

    if (!CFG.EnableStartupCrop)
    {
        for (const auto& [img, _] : img_dict)
        {
            if (img == CFG.FallbackName)
            {
                continue;
            }

            if (!std::ranges::contains(crop_list, img))
            {
                return true;
            }
        }
    }

    return !img_dict.contains(CFG.FallbackName);
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

    const fs::path fallback_img{ CFG.FallbackName };
    if (img_dict.contains(fallback_img) && img_dict.at(fallback_img).UncroppedImage.Width() == CFG.BasePreviewWidth)
    {
        out_img_dict[fallback_img] = img_dict.at(fallback_img);
    }

    const PixelSize uncropped_size{ CFG.BasePreviewWidth, dla::math::round(CFG.BasePreviewWidth / CardRatio) };

    {
        const bool has_img{ out_img_dict.contains(fallback_img) };
        if (!has_img)
        {
            PPP_LOG("Caching fallback image {}...", fallback_img.string());
            ImagePreview& image_preview{ out_img_dict[fallback_img] };
            image_preview.UncroppedImage = Image::Read(fallback_img);
            image_preview.CroppedImage = CropImage(image_preview.UncroppedImage, fallback_img, 0_mm, 1200_dpi, nullptr);
        }
    }

    const PixelSize cropped_size{ out_img_dict[fallback_img].CroppedImage.Size() };

    std::vector input_files{ ListImageFiles(image_dir) };
    if (CFG.EnableUncrop)
    {
        for (const auto& cropped_img : ListImageFiles(crop_dir))
        {
            if (!std::ranges::contains(input_files, cropped_img))
            {
                input_files.push_back(cropped_img);
            }
        }
    }

    for (const auto& img : input_files)
    {
        const bool has_img{ out_img_dict.contains(img) };
        if (has_img)
        {
            continue;
        }

        if (fs::exists(image_dir / img))
        {
            const Image image{ Image::Read(image_dir / img) };

            ImagePreview& image_preview{ out_img_dict[img] };

            PPP_LOG("Caching uncropped preview for image {}...", img.string());
            image_preview.UncroppedImage = image.Resize(uncropped_size);

            PPP_LOG("Caching cropped preview for image {}...", img.string());
            image_preview.CroppedImage = CropImage(image_preview.UncroppedImage, img, 0_mm, 1200_dpi, nullptr);
        }
        else if (CFG.EnableUncrop && fs::exists(crop_dir / img))
        {
            PPP_LOG("Only approximate preview available for image {}...", img.string());

            const Image image{ Image::Read(crop_dir / img) };

            ImagePreview& image_preview{ out_img_dict[img] };

            PPP_LOG("Caching cropped preview for image {}...", img.string());
            image_preview.CroppedImage = image.Resize(cropped_size);

            PPP_LOG("Caching uncropped preview for image {}...", img.string());
            image_preview.UncroppedImage = UncropImage(image_preview.CroppedImage, img, nullptr);
        }
        else
        {
            PPP_LOG("Failed caching uncropped preview for image {}...", img.string());
        }
    }

    WritePreviews(img_cache_file, out_img_dict);

    return out_img_dict;
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

                img_dict[img_name] = std::move(img);
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
                const std::vector buf{ image.CroppedImage.Encode() };
                write_arr(buf.data(), buf.size());
            }
            {
                const std::vector buf{ image.UncroppedImage.Encode() };
                write_arr(buf.data(), buf.size());
            }
        }
    }
}

cv::Mat LoadColorCube(const fs::path& file_path)
{
    QFile color_cube_file{ ToQString(file_path) };
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
