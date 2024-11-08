#pragma once

#include <filesystem>
#include <vector>

#include <opencv2/core/mat.hpp>

#include <ppp/util.hpp>

class Image
{
  public:
    Image() = default;
    ~Image();

    Image(Image&& rhs);
    Image(const Image& rhs);

    Image& operator=(Image&& rhs);
    Image& operator=(const Image& rhs);

    static Image Read(const fs::path& path);

    bool Write(const fs::path& path) const;

    explicit operator bool() const;
    bool Valid() const;

    enum class Rotation
    {
        Degree90,
        Degree180,
        Degree270,
    };
    Image Rotate(Rotation rotation) const;

    Image Crop(Pixel left, Pixel top, Pixel right, Pixel bottom) const;
    Image AddBlackBorder(Pixel left, Pixel top, Pixel right, Pixel bottom) const;

    Image Resize(PixelSize size) const;

    PixelSize Size() const;
    PixelDensity Density(::Size real_size) const;

  private:
    void Release();

    cv::Mat m_Impl{};
};
