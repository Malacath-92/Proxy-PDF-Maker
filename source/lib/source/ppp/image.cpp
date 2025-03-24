#include <ppp/image.hpp>

#include <ranges>

#include <dla/scalar_math.h>

#include <opencv2/img_hash.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#include <QPixmap>

#include <ppp/color.hpp>

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
    img.m_Impl = cv::imread(path.string().c_str());
    return img;
}

bool Image::Write(const fs::path& path) const
{
    cv::imwrite(path.string().c_str(), m_Impl);
    return true;
}

Image Image::Decode(const std::vector<std::byte>& buffer)
{
    Image img{};
    const cv::InputArray cv_buffer{ reinterpret_cast<const uchar*>(buffer.data()), static_cast<int>(buffer.size()) };
    img.m_Impl = cv::imdecode(cv_buffer, cv::IMREAD_COLOR);
    return img;
}

std::vector<std::byte> Image::Encode() const
{
    std::vector<uchar> cv_buffer;
    if (cv::imencode(".png", m_Impl, cv_buffer))
    {
        std::vector<std::byte> out_buffer(cv_buffer.size(), std::byte{});
        std::memcpy(out_buffer.data(), cv_buffer.data(), cv_buffer.size());
        return out_buffer;
    }
    return {};
}

QPixmap Image::StoreIntoQtPixmap() const
{
    return QPixmap::fromImage(QImage(m_Impl.ptr(), m_Impl.cols, m_Impl.rows, m_Impl.step, QImage::Format_BGR888));
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
    Image img{};
    img.m_Impl = m_Impl(
        cv::Range(static_cast<int>(top.value), static_cast<int>((h - bottom).value)),
        cv::Range(static_cast<int>(left.value), static_cast<int>((w - right).value)));
    return img;
}

Image Image::AddBlackBorder(Pixel left, Pixel top, Pixel right, Pixel bottom) const
{
    Image img{};
    cv::copyMakeBorder(m_Impl,
                       img.m_Impl,
                       static_cast<int>(top.value),
                       static_cast<int>(bottom.value),
                       static_cast<int>(left.value),
                       static_cast<int>(right.value),
                       cv::BORDER_CONSTANT,
                       0xFFFFFFFF);
    return img;
}

Image Image::ApplyColorCube(const cv::Mat& color_cube) const
{
    const int cube_size_minus_one{ color_cube.cols - 1 };

    Image filtered{ *this };
    auto flat_range{ std::views::iota(0, m_Impl.rows * m_Impl.cols) };
    std::for_each(flat_range.begin(), flat_range.end(), [&](int i)
                  {
                      const int x{ i % m_Impl.rows };
                      const int y{ i / m_Impl.rows };
                      const auto& col{ m_Impl.at<cv::Vec3b>(x, y) };

                      const float r{ (static_cast<float>(col[2]) / 255) * cube_size_minus_one };
                      const float g{ (static_cast<float>(col[1]) / 255) * cube_size_minus_one };
                      const float b{ (static_cast<float>(col[0]) / 255) * cube_size_minus_one };

        // clang-format off
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

                      static constexpr auto linear_interpolate{
                          [](std::array<ColorRGB32f, 2> values, float alpha)
                          {
                              return values[0] * (1 - alpha) + values[1] * alpha;
                          },
                      };

                      static constexpr auto bilinear_interpolate{
                          [](std::array<ColorRGB32f, 4> values, std::array<float, 2> alphas)
                          {
                              const std::array interim_values{
                                  linear_interpolate(std::array{ values[0], values[1] }, alphas[0]),
                                  linear_interpolate(std::array{ values[2], values[3] }, alphas[0]),
                              };
                              return linear_interpolate(interim_values, alphas[1]);
                          },
                      };

                      static constexpr auto trilinear_interpolate{
                          [](std::array<ColorRGB32f, 8> values, std::array<float, 3> alphas)
                          {
                              const std::array interim_values{
                                  bilinear_interpolate(std::array{ values[0], values[1], values[2], values[3] }, std::array{ alphas[0], alphas[1] }),
                                  bilinear_interpolate(std::array{ values[4], values[5], values[6], values[7] }, std::array{ alphas[0], alphas[1] }),
                              };
                              return linear_interpolate(interim_values, alphas[2]);
                          },
                      };

                      auto color_at{
                          [&](int r, int g, int b)
                          {
                              const auto v{ color_cube.at<cv::Vec3b>(r, g, b) };
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

                      const ColorRGB32f interpolated{ trilinear_interpolate(corners, { r_frac, g_frac, b_frac }) };
                      filtered.m_Impl.at<cv::Vec3b>(x, y) = cv::Vec3b{
                          static_cast<uchar>(interpolated.r),
                          static_cast<uchar>(interpolated.g),
                          static_cast<uchar>(interpolated.b),
                      };
#else
                      // In Debug we just get the nearest element
                      const auto res{ color_cube.at<cv::Vec3b>((int)r, (int)g, (int)b) };
                      filtered.m_Impl.at<cv::Vec3b>(x, y) = res;
#endif
                      // clang-format on
                  });
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
