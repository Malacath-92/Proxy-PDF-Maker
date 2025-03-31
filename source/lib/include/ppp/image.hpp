#pragma once

#include <filesystem>
#include <vector>

#include <opencv2/core/mat.hpp>

#include <ppp/util.hpp>

class QPixmap;

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
    bool Write(const fs::path& path, std::optional<int32_t> compression = std::nullopt) const;
    bool Write(const fs::path& path, std::optional<int32_t> compression, Size dimensions) const;

    static Image Decode(const std::vector<std::byte>& buffer);
    std::vector<std::byte> Encode(std::optional<int32_t> compression = std::nullopt) const;

    std::vector<std::byte> EncodePng(std::optional<int32_t> compression = std::nullopt) const;
    std::vector<std::byte> EncodeJpg(std::optional<int32_t> quality = std::nullopt) const;

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

    Image ApplyColorCube(const cv::Mat& color_cube) const;

    Image Resize(PixelSize size) const;

    Pixel Width() const;
    Pixel Height() const;
    PixelSize Size() const;
    PixelDensity Density(::Size real_size) const;

    uint64_t Hash() const;

    const cv::Mat& GetUnderlying() const;

    void DebugDisplay() const;

  private:
    void Release();

    cv::Mat m_Impl{};
};
