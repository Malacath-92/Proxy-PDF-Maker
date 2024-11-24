#include <ppp/project/image_ops.hpp>

#include <array>

#include <dla/fmt_format.h>
#include <dla/scalar_math.h>
#include <dla/vector_math.h>

#include <ppp/constants.hpp>

#include <ppp/version.hpp>

void InitImageSystem()
{
}

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
        return crop_dir / "vibrance";
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
                   bool do_vibrance_bump,
                   bool uncrop,
                   PrintFn print_fn)
{
    ImgDict out_img_dict{ img_dict };

    const bool has_bleed_edge{ bleed_edge > 0_mm };
    if (has_bleed_edge)
    {
        // Do base cropping, always needed
        out_img_dict = RunCropper(image_dir, crop_dir, img_cache_file, out_img_dict, 0_mm, max_density, do_vibrance_bump, uncrop, print_fn);
    }
    else if (do_vibrance_bump)
    {
        // Do base cropping without vibrance for previews
        out_img_dict = RunCropper(image_dir, crop_dir, img_cache_file, out_img_dict, 0_mm, max_density, false, uncrop, print_fn);
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
            throw std::logic_error{ "Not implemented..." };
        }
        cropped_image.Write(output_dir / img_file);
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
        if (!img_dict.contains(img))
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
        if (fs::exists(crop_dir / img) && img_dict.contains(img))
        {
            out_img_dict[img] = img_dict.at(img);
        }
    }
    if (img_dict.contains("fallback.png"))
    {
        out_img_dict["fallback.png"] = img_dict.at("fallback.png");
    }

    const std::vector input_files{ ListImageFiles(image_dir) };
    for (const auto& img : input_files)
    {
        const bool has_img{ out_img_dict.contains(img) };
        if (has_img)
        {
            continue;
        }

        ImagePreview image_preview{};

        {
            const Image image{ Image::Read(image_dir / img) };
            const auto [w, h]{ image.Size().pod() };

            {
                const float preview_scale{ 248_pix / w };
                const PixelSize preview_size{ dla::math::round(w * preview_scale), dla::math::round(h * preview_scale) };

                PPP_LOG("Caching preview for image {}...", img.string());
                image_preview.CroppedImage = image.Resize(preview_size);
            }

            {
                const float thumb_scale{ 124_pix / w };
                const PixelSize thumb_size{ dla::math::round(w * thumb_scale), dla::math::round(h * thumb_scale) };

                PPP_LOG("Caching thumbnail for image {}...", img.string());
                image_preview.CroppedThumbImage = image.Resize(thumb_size);
            }
        }

        if (fs::exists(crop_dir / img))
        {
            const Image image{ Image::Read(crop_dir / img) };
            const auto [w, h]{ image.Size().pod() };

            const float uncropped_scale{ 186_pix / w };
            const PixelSize uncropped_size{ dla::math::round(w * uncropped_scale), dla::math::round(h * uncropped_scale) };

            PPP_LOG("Caching uncropped preview for image {}...", img.string());
            image_preview.UncroppedImage = image.Resize(uncropped_size);
        }
        else
        {
            PPP_LOG("Failed caching uncropped preview for image {}...", img.string());
        }

        out_img_dict[img] = std::move(image_preview);
    }

    {
        const fs::path fallback_img{ "fallback.png" };
        const bool has_img{ out_img_dict.contains(fallback_img) };
        if (!has_img)
        {
            PPP_LOG("Caching fallback image {}...", fallback_img.string());
            ImagePreview& image_preview{ out_img_dict[fallback_img] };
            image_preview.CroppedImage = Image::Read(fallback_img);
            image_preview.CroppedThumbImage = image_preview.CroppedImage.Resize({ 124_pix, 160_pix });
            image_preview.UncroppedImage = image_preview.CroppedImage.Resize({ 186_pix, 242_pix });
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
