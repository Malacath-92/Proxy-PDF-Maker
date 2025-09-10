#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include <opencv2/core/mat.hpp>

#include <ppp/color.hpp>
#include <ppp/util.hpp>

class QPixmap;

using EncodedImage = std::vector<std::byte>;
using EncodedImageView = std::span<const std::byte>;

class [[nodiscard]] Image
{
  public:
    Image() = default;
    Image(cv::Mat impl);
    ~Image();

    Image(Image&& rhs);
    Image(const Image& rhs);

    Image& operator=(Image&& rhs);
    Image& operator=(const Image& rhs);

    static Image Read(const fs::path& path);
    bool Write(const fs::path& path, std::optional<int32_t> png_compression = std::nullopt, std::optional<int32_t> jpg_quality = std::nullopt) const;
    bool Write(const fs::path& path, std::optional<int32_t> png_compression, std::optional<int32_t> jpg_quality, Size dimensions) const;

    static Image Decode(const EncodedImage& buffer);
    static Image Decode(EncodedImageView buffer);

    EncodedImage EncodePng(std::optional<int32_t> compression = std::nullopt) const;
    EncodedImage EncodeJpg(std::optional<int32_t> quality = std::nullopt) const;

    QPixmap StoreIntoQtPixmap() const;

    explicit operator bool() const;
    bool Valid() const;

    enum class Rotation
    {
        None,
        Degree90,
        Degree180,
        Degree270,
    };
    Image Rotate(Rotation rotation) const;

    Image Crop(Pixel left, Pixel top, Pixel right, Pixel bottom) const;
    Image AddBlackBorder(Pixel left, Pixel top, Pixel right, Pixel bottom) const;
    Image AddReflectBorder(Pixel left, Pixel top, Pixel right, Pixel bottom) const;

    Image ApplyAlpha(const ColorRGB8& color) const;

    Image RoundCorners(::Size real_size, ::Length corner_radius) const;

    Image FillCorners(::Length corner_radius, ::Size physical_size) const;

    Image ApplyColorCube(const cv::Mat& color_cube) const;

    Image Resize(PixelSize size) const;

    Pixel Width() const;
    Pixel Height() const;
    PixelSize Size() const;
    float AspectRatio() const;
    PixelDensity Density(::Size real_size) const;

    uint64_t Hash() const;

    const cv::Mat& GetUnderlying() const;

    void DebugDisplay() const;

  private:
    void Release();

    cv::Mat m_Impl{};
};
