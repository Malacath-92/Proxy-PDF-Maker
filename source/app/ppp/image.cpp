#include <ppp/image.hpp>

#include <dla/scalar_math.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

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
    m_Impl = rhs.m_Impl;
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

Image Image::Resize(PixelSize size) const
{
    Image img{};
    cv::resize(m_Impl, img.m_Impl, cv::Size(static_cast<int>(size.x.value), static_cast<int>(size.y.value)), cv::INTER_CUBIC);
    return img;
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
    const auto [w, h] = Size().pod();
    const auto [bw, bh] = (real_size).pod();
    return dla::math::min(w / bw, h / bh);
}

void Image::Release()
{
    m_Impl = cv::Mat{};
}
