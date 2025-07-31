#include <ppp/image.hpp>

#include <bit>
#include <ranges>

#include <dla/scalar_math.h>

#include <opencv2/img_hash.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#include <QPixmap>

#include <ppp/color.hpp>

namespace pngcrc
{
static uint32_t CRC(const uchar* buf, int len)
{
    static constexpr auto c_CrcTable{
        []()
        {
            std::array<uint32_t, 256> crc_table_bld{};
            for (int32_t n = 0; n < 256; n++)
            {
                uint32_t c{ static_cast<uint32_t>(n) };
                for (int32_t k = 0; k < 8; k++)
                {
                    if (c & 1)
                    {
                        c = 0xedb88320L ^ (c >> 1);
                    }
                    else
                    {
                        c = c >> 1;
                    }
                }
                crc_table_bld[n] = c;
            }
            return crc_table_bld;
        }()
    };
    static constexpr auto c_CrcImpl{
        [](uint32_t crc, const uchar* buf, int len)
        {
            uint32_t c{ crc };
            for (int32_t n = 0; n < len; n++)
            {
                c = c_CrcTable[(c ^ buf[n]) & 0xff] ^ (c >> 8);
            }
            return c;
        }
    };
    return c_CrcImpl(0xffffffffL, buf, len) ^ 0xffffffffL;
}
} // namespace pngcrc

Image::Image(cv::Mat impl)
    : m_Impl{ std::move(impl) }
{
}

Image::~Image()
{
    Release();
}

Image::Image(Image&& rhs)
{
    *this = std::move(rhs);
}
Image::Image(const Image& rhs)
{
    *this = rhs;
}

Image& Image::operator=(Image&& rhs)
{
    m_Impl = std::move(rhs.m_Impl);
    return *this;
}
Image& Image::operator=(const Image& rhs)
{
    m_Impl = rhs.m_Impl.clone();
    return *this;
}

Image Image::Read(const fs::path& path)
{
    Image img{};
    img.m_Impl = cv::imread(path.string().c_str(), cv::IMREAD_UNCHANGED);
    return img;
}

bool Image::Write(const fs::path& path, std::optional<int32_t> png_compression, std::optional<int32_t> jpg_quality) const
{
    const fs::path ext{ path.extension() };
    if (ext == ".png")
    {
        std::vector<int> png_params;
        if (png_compression.has_value())
        {
            png_params = {
                cv::IMWRITE_PNG_COMPRESSION,
                png_compression.value(),
                cv::IMWRITE_PNG_STRATEGY,
                cv::IMWRITE_PNG_STRATEGY_DEFAULT,
            };
        }

        return cv::imwrite(path.string().c_str(), m_Impl, png_params);
    }
    else if (ext == ".jpg")
    {
        std::vector<int> jpg_params;
        if (jpg_quality.has_value())
        {
            jpg_params = {
                cv::IMWRITE_JPEG_QUALITY,
                jpg_quality.value(),
            };
        }

        return cv::imwrite(path.string().c_str(), m_Impl, jpg_params);
    }
    else
    {
        return cv::imwrite(path.string().c_str(), m_Impl);
    }
}

bool Image::Write(const fs::path& path, std::optional<int32_t> png_compression, std::optional<int32_t> jpg_quality, ::Size dimensions) const
{
    const fs::path ext{ path.extension() };
    if (ext == ".png")
    {
        std::vector<int> png_params;
        if (png_compression.has_value())
        {
            png_params = {
                cv::IMWRITE_PNG_COMPRESSION,
                png_compression.value(),
                cv::IMWRITE_PNG_STRATEGY,
                cv::IMWRITE_PNG_STRATEGY_DEFAULT,
            };
        }

        const PixelDensity density{ Density(dimensions) };

        std::vector<uchar> buf;
        if (cv::imencode(".png", m_Impl, buf, png_params))
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

                // Create pHYs chunk...
                struct
                {
                    uint32_t m_Size;
                    std::array<char, 4> m_Name;
                    uint32_t m_DotsPerMeterX;
                    uint32_t m_DotsPerMeterY;
                    uint8_t m_Unit;
                } const phys_chunk{
                    std::byteswap(9u),
                    { 'p', 'H', 'Y', 's' },
                    std::byteswap(static_cast<uint32_t>(density.value)),
                    std::byteswap(static_cast<uint32_t>(density.value)),
                    1, // this just means meter
                };

                // size + name + data + padding
                static constexpr uint32_t c_PhysChunkSize{ 4 + 4 + 9 };
                // chunk + padding
                static_assert(sizeof(phys_chunk) == c_PhysChunkSize + 3);
                // verify padding is at the end ...
                static_assert(offsetof(decltype(phys_chunk), m_Unit) == c_PhysChunkSize - 1);

                // only the name and data
                const auto* crc_data{ reinterpret_cast<const uchar*>(&phys_chunk) + 4 };
                const auto crc_data_size{ c_PhysChunkSize - 4 };
                const uint32_t crc{ std::byteswap(pngcrc::CRC(crc_data, crc_data_size)) };

                // chunk + crc
                std::array<uchar, c_PhysChunkSize + 4> phys_buf;
                std::memcpy(phys_buf.data(), &phys_chunk, c_PhysChunkSize);
                std::memcpy(phys_buf.data() + c_PhysChunkSize, &crc, 4);
                buf.insert(buf.begin() + idat_idx, phys_buf.begin(), phys_buf.end());
            }

            if (FILE * file{ fopen(path.string().c_str(), "wb") })
            {
                fwrite(buf.data(), 1, buf.size(), file);
                fclose(file);
            }

            cv::imdecode(buf, cv::IMREAD_UNCHANGED);

            return true;
        }

        return false;
    }
    else if (ext == ".jpg" || ext == ".jpeg" || ext == ".jpe")
    {
        std::vector<int> jpg_params;
        if (jpg_quality.has_value())
        {
            jpg_params = {
                cv::IMWRITE_JPEG_QUALITY,
                jpg_quality.value(),
            };
        }

        const PixelDensity dpi{ Density(dimensions) * 1_in / 1_m };

        std::vector<uchar> buf;
        if (cv::imencode(".jpg", m_Impl, buf, jpg_params))
        {
            {
                // Write the pixel density into the APP0 chunk that OpenCV wrote...
                assert(buf[0] == 0xff);
                assert(buf[1] == 0xd8);
                assert(buf[2] == 0xff);
                assert(buf[3] == 0xe0);

                // Not declaring a struct here because I can't be arsed to pack it tightly
                uint8_t* units{ reinterpret_cast<uint8_t*>(buf.data() + 4 + 9) };
                uint16_t* x_density{ reinterpret_cast<uint16_t*>(buf.data() + 4 + 9 + 1) };
                uint16_t* y_density{ reinterpret_cast<uint16_t*>(buf.data() + 4 + 9 + 1 + 2) };

                *units = 1; // 0: pixel ratio only, 1: DPI, 2: dots per cm
                *x_density = std::byteswap(static_cast<uint16_t>(dpi.value));
                *y_density = *x_density;
            }

            if (FILE * file{ fopen(path.string().c_str(), "wb") })
            {
                fwrite(buf.data(), 1, buf.size(), file);
                fclose(file);
            }

            return true;
        }

        return false;
    }
    else
    {
        return cv::imwrite(path.string(), m_Impl);
    }
}

Image Image::Decode(const EncodedImage& buffer)
{
    return Decode(EncodedImageView{ buffer });
}

Image Image::Decode(EncodedImageView buffer)
{
    Image img{};
    cv::InputArray cv_buffer{ reinterpret_cast<const uchar*>(buffer.data()),
                              static_cast<int>(buffer.size()) };
    img.m_Impl = cv::imdecode(cv_buffer, cv::IMREAD_UNCHANGED);
    return img;
}

EncodedImage Image::EncodePng(std::optional<int32_t> compression) const
{
    // Safety check: if image is empty, return empty buffer
    if (m_Impl.empty())
    {
        return {};
    }
    
    std::vector<int> png_params;
    if (compression.has_value())
    {
        png_params = {
            cv::IMWRITE_PNG_COMPRESSION,
            compression.value(),
            cv::IMWRITE_PNG_STRATEGY,
            cv::IMWRITE_PNG_STRATEGY_DEFAULT,
        };
    }

    std::vector<uchar> cv_buffer;
    if (cv::imencode(".png", m_Impl, cv_buffer, png_params))
    {
        EncodedImage out_buffer(cv_buffer.size(), std::byte{});
        std::memcpy(out_buffer.data(), cv_buffer.data(), cv_buffer.size());
        return out_buffer;
    }
    return {};
}

EncodedImage Image::EncodeJpg(std::optional<int32_t> quality) const
{
    // Safety check: if image is empty, return empty buffer
    if (m_Impl.empty())
    {
        return {};
    }
    
    std::vector<int> jpg_params;
    if (quality.has_value())
    {
        jpg_params = {
            cv::IMWRITE_JPEG_QUALITY,
            quality.value(),
        };
    }

    std::vector<uchar> cv_buffer;
    if (cv::imencode(".jpg", m_Impl, cv_buffer, jpg_params))
    {
        EncodedImage out_buffer(cv_buffer.size(), std::byte{});
        std::memcpy(out_buffer.data(), cv_buffer.data(), cv_buffer.size());
        return out_buffer;
    }
    return {};
}

QPixmap Image::StoreIntoQtPixmap() const
{
    switch (m_Impl.channels())
    {
    case 1:
        return QPixmap::fromImage(QImage(m_Impl.ptr(), m_Impl.cols, m_Impl.rows, m_Impl.step, QImage::Format_Grayscale8));
    case 3:
        return QPixmap::fromImage(QImage(m_Impl.ptr(), m_Impl.cols, m_Impl.rows, m_Impl.step, QImage::Format_BGR888));
    case 4:
    {
        cv::Mat img;
        cv::cvtColor(m_Impl, img, cv::COLOR_BGR2RGBA);
        return QPixmap::fromImage(QImage(img.ptr(), img.cols, img.rows, img.step, QImage::Format_RGBA8888));
    }
    default:
        return QPixmap{ static_cast<int>(Width() / 1_pix), static_cast<int>(Height() / 1_pix) };
    }
}

Image::operator bool() const
{
    return !m_Impl.empty();
}

bool Image::Valid() const
{
    return static_cast<bool>(*this);
}

Image Image::Rotate(Rotation rotation) const
{
    Image img{};
    switch (rotation)
    {
    case Rotation::Degree90:
        cv::rotate(m_Impl, img.m_Impl, cv::ROTATE_90_CLOCKWISE);
        break;
    case Rotation::Degree180:
        cv::rotate(m_Impl, img.m_Impl, cv::ROTATE_180);
        break;
    case Rotation::Degree270:
        cv::rotate(m_Impl, img.m_Impl, cv::ROTATE_90_COUNTERCLOCKWISE);
        break;
    default:
        img = *this;
    }
    return img;
}

Image Image::Crop(Pixel left, Pixel top, Pixel right, Pixel bottom) const
{
    const auto [w, h] = Size().pod();
    
    // Ensure crop parameters are valid to prevent crashes
    const int safe_left = static_cast<int>(std::max(0, static_cast<int>(left.value)));
    const int safe_top = static_cast<int>(std::max(0, static_cast<int>(top.value)));
    const int safe_right = static_cast<int>(std::max(0, static_cast<int>(right.value)));
    const int safe_bottom = static_cast<int>(std::max(0, static_cast<int>(bottom.value)));
    
    // Ensure ranges are valid
    const int end_y = static_cast<int>(h.value) - safe_bottom;
    const int end_x = static_cast<int>(w.value) - safe_right;
    
    // If crop would result in empty image, return empty image
    if (safe_top >= end_y || safe_left >= end_x)
    {
        return Image{};
    }
    
    Image img{};
    img.m_Impl = m_Impl(
        cv::Range(safe_top, end_y),
        cv::Range(safe_left, end_x));
    return img;
}

Image Image::AddBlackBorder(Pixel left, Pixel top, Pixel right, Pixel bottom) const
{
    // Ensure border parameters are valid to prevent crashes
    const int safe_left = static_cast<int>(std::max(0, static_cast<int>(left.value)));
    const int safe_top = static_cast<int>(std::max(0, static_cast<int>(top.value)));
    const int safe_right = static_cast<int>(std::max(0, static_cast<int>(right.value)));
    const int safe_bottom = static_cast<int>(std::max(0, static_cast<int>(bottom.value)));
    
    Image img{};
    cv::copyMakeBorder(m_Impl,
                       img.m_Impl,
                       safe_top,
                       safe_bottom,
                       safe_left,
                       safe_right,
                       cv::BORDER_CONSTANT,
                       0xFFFFFFFF);
    return img;
}

Image Image::AddReflectBorder(Pixel left, Pixel top, Pixel right, Pixel bottom) const
{
    // Ensure border parameters are valid to prevent crashes
    const int safe_left = static_cast<int>(std::max(0, static_cast<int>(left.value)));
    const int safe_top = static_cast<int>(std::max(0, static_cast<int>(top.value)));
    const int safe_right = static_cast<int>(std::max(0, static_cast<int>(right.value)));
    const int safe_bottom = static_cast<int>(std::max(0, static_cast<int>(bottom.value)));
    
    Image img{};
    cv::copyMakeBorder(m_Impl,
                       img.m_Impl,
                       safe_top,
                       safe_bottom,
                       safe_left,
                       safe_right,
                       cv::BORDER_REFLECT,
                       0xFFFFFFFF);
    return img;
}

Image Image::RoundCorners(::Size real_size, ::Length corner_radius) const
{
    if (corner_radius == 1_mm)
    {
        return *this;
    }

    const auto corner_radius_pixels{ static_cast<int>(dla::math::floor(Density(real_size) * corner_radius) / 1_pix) };
    if (corner_radius_pixels == 0)
    {
        return *this;
    }

    cv::Mat mask{ m_Impl.rows, m_Impl.cols, CV_8UC1, cv::Scalar{ 0 } };
    {
        const cv::Point top_left_wide{ 0, corner_radius_pixels };
        const cv::Point bottom_right_wide{ m_Impl.cols, m_Impl.rows - corner_radius_pixels };
        const cv::Point top_left_tall{ corner_radius_pixels, 0 };
        const cv::Point bottom_right_tall{ m_Impl.cols - corner_radius_pixels, m_Impl.rows };
        const auto draw_rect{
            [&](const cv::Point& top_left, const cv::Point& bottom_right)
            {
                cv::rectangle(mask, top_left, bottom_right, cv::Scalar(255), cv::FILLED);
            },
        };
        draw_rect(top_left_wide, bottom_right_wide);
        draw_rect(top_left_tall, bottom_right_tall);

        const cv::Point top_left{ corner_radius_pixels, corner_radius_pixels };
        const cv::Point bottom_left{ m_Impl.cols - 1 - corner_radius_pixels, corner_radius_pixels };
        const cv::Point bottom_right{ m_Impl.cols - 1 - corner_radius_pixels, m_Impl.rows - 1 - corner_radius_pixels };
        const cv::Point top_right{ corner_radius_pixels, m_Impl.rows - 1 - corner_radius_pixels };
        const auto draw_arc{
            [&](const cv::Point& pos)
            {
                cv::circle(mask, pos, corner_radius_pixels, cv::Scalar(255), cv::FILLED, cv::LINE_AA);
            },
        };
        draw_arc(top_left);
        draw_arc(bottom_left);
        draw_arc(bottom_right);
        draw_arc(top_right);
    }

    std::vector<cv::Mat> out_channels;
    cv::split(m_Impl, out_channels);
    if (out_channels.size() != 4)
    {
        out_channels.push_back(std::move(mask));
    }
    else
    {
        cv::multiply(mask, out_channels[3], out_channels[3]);
    }

    cv::Mat out_impl;
    cv::merge(out_channels, out_impl);
    return Image{ out_impl };
}

template<int Channels>
struct CubeFilter
{
    const cv::Mat& m_ColorCube;
    const cv::Mat& m_InputImg;
    cv::Mat& m_OutputImg;

    using element_t = std::conditional_t<Channels == 3, cv::Vec3b, cv::Vec4b>;

    void operator()(int idx)
    {
        const int x{ idx % m_InputImg.rows };
        const int y{ idx / m_InputImg.rows };
        const auto& col{ m_InputImg.at<element_t>(x, y) };

        auto& out_element{ m_OutputImg.at<element_t>(x, y) };
        out_element = col;

        const int cube_size_minus_one{ m_ColorCube.cols - 1 };
        const float r{ (static_cast<float>(col[2]) / 255) * cube_size_minus_one };
        const float g{ (static_cast<float>(col[1]) / 255) * cube_size_minus_one };
        const float b{ (static_cast<float>(col[0]) / 255) * cube_size_minus_one };

#ifdef NDEBUG
        // In Release we interpolate between the eight cube-elements

        const int r_lo{ static_cast<int>(std::floor(r)) };
        const int r_hi{ static_cast<int>(std::ceil(r)) };
        const float r_frac{ r - static_cast<float>(r_lo) };

        const int g_lo{ static_cast<int>(std::floor(g)) };
        const int g_hi{ static_cast<int>(std::ceil(g)) };
        const float g_frac{ g - static_cast<float>(g_lo) };

        const int b_lo{ static_cast<int>(std::floor(b)) };
        const int b_hi{ static_cast<int>(std::ceil(b)) };
        const float b_frac{ b - static_cast<float>(b_lo) };

        static constexpr auto c_LinearInterpolate{
            [](std::array<ColorRGB32f, 2> values, float alpha)
            {
                return values[0] * (1 - alpha) + values[1] * alpha;
            },
        };

        static constexpr auto c_BilinearInterpolate{
            [](std::array<ColorRGB32f, 4> values, std::array<float, 2> alphas)
            {
                const std::array interim_values{
                    c_LinearInterpolate(std::array{ values[0], values[1] }, alphas[0]),
                    c_LinearInterpolate(std::array{ values[2], values[3] }, alphas[0]),
                };
                return c_LinearInterpolate(interim_values, alphas[1]);
            },
        };

        static constexpr auto c_TrilinearInterpolate{
            [](std::array<ColorRGB32f, 8> values, std::array<float, 3> alphas)
            {
                const std::array interim_values{
                    c_BilinearInterpolate(std::array{ values[0], values[1], values[2], values[3] }, std::array{ alphas[0], alphas[1] }),
                    c_BilinearInterpolate(std::array{ values[4], values[5], values[6], values[7] }, std::array{ alphas[0], alphas[1] }),
                };
                return c_LinearInterpolate(interim_values, alphas[2]);
            },
        };

        auto color_at{
            [&](int r, int g, int b)
            {
                const auto v{ m_ColorCube.at<cv::Vec3b>(r, g, b) };
                return ColorRGB32f{
                    static_cast<float>(v[0]),
                    static_cast<float>(v[1]),
                    static_cast<float>(v[2]),
                };
            },
        };

        std::array corners{
            color_at(r_lo, g_lo, b_lo),
            color_at(r_hi, g_lo, b_lo),

            color_at(r_lo, g_hi, b_lo),
            color_at(r_hi, g_hi, b_lo),

            color_at(r_lo, g_lo, b_hi),
            color_at(r_hi, g_lo, b_hi),

            color_at(r_lo, g_hi, b_hi),
            color_at(r_hi, g_hi, b_hi),
        };

        const ColorRGB32f interpolated{ c_TrilinearInterpolate(corners, { r_frac, g_frac, b_frac }) };
        out_element[0] = static_cast<uchar>(interpolated.r);
        out_element[1] = static_cast<uchar>(interpolated.g);
        out_element[2] = static_cast<uchar>(interpolated.b);
#else
        // In Debug we just get the nearest element
        const auto res{ m_ColorCube.at<cv::Vec3b>((int)r, (int)g, (int)b) };
        out_element[0] = static_cast<uchar>(res[0]);
        out_element[1] = static_cast<uchar>(res[1]);
        out_element[2] = static_cast<uchar>(res[2]);
#endif
    }
};

Image Image::ApplyColorCube(const cv::Mat& color_cube) const
{
    if (m_Impl.channels() == 1)
    {
        return *this;
    }

    Image filtered{ *this };
    auto flat_range{ std::views::iota(0, m_Impl.rows * m_Impl.cols) };
    switch (m_Impl.channels())
    {
    case 3:
        std::for_each(flat_range.begin(), flat_range.end(), CubeFilter<3>{ color_cube, m_Impl, filtered.m_Impl });
        break;
    case 4:
        std::for_each(flat_range.begin(), flat_range.end(), CubeFilter<4>{ color_cube, m_Impl, filtered.m_Impl });
        break;
    default:
        break;
    }
    return filtered;
}

Image Image::Resize(PixelSize size) const
{
    Image img{};
    cv::resize(m_Impl, img.m_Impl, cv::Size(static_cast<int>(size.x.value), static_cast<int>(size.y.value)), cv::INTER_AREA);
    return img;
}

Pixel Image::Width() const
{
    return Size().x;
}

Pixel Image::Height() const
{
    return Size().y;
}

PixelSize Image::Size() const
{
    return ::PixelSize{
        Pixel(static_cast<float>(m_Impl.cols)),
        Pixel(static_cast<float>(m_Impl.rows)),
    };
}

PixelDensity Image::Density(::Size real_size) const
{
    const auto [w, h]{ Size().pod() };
    const auto [bw, bh]{ (real_size).pod() };
    
    // Safety check: if real_size is very small or zero, return a reasonable default
    if (bw <= 0_m || bh <= 0_m)
    {
        return PixelDensity{ 96.0f }; // Default DPI value
    }
    
    return dla::math::min(w / bw, h / bh);
}

uint64_t Image::Hash() const
{
    cv::Mat hash;
    cv::img_hash::pHash(m_Impl, hash);
    return *reinterpret_cast<uint64_t*>(hash.data);
}

const cv::Mat& Image::GetUnderlying() const
{
    return m_Impl;
}

void Image::DebugDisplay() const
{
    cv::imshow("Debug Display", m_Impl);
    cv::waitKey();
}

void Image::Release()
{
    m_Impl = cv::Mat{};
}
