#pragma once

#include <filesystem>
#include <vector>

#include <vips/basic.h>

#include <ppp/util.hpp>

class QPixmap;

class Image;
using ColorCube = std::vector<Image>;

class [[nodiscard]] Image
{
  public:
    Image() = default;
    ~Image();

    Image(Image&& rhs);
    Image(const Image& rhs);

    Image& operator=(Image&& rhs);
    Image& operator=(const Image& rhs);

    static Image FromMemory(const void* data, int width, int height);

    static Image Read(const fs::path& path);
    bool Write(const fs::path& path) const;

    static Image Decode(const std::vector<std::byte>& buffer);
    std::vector<std::byte> Encode() const;

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

    Image ApplyColorCube(const ColorCube& color_cube) const;

    Image Resize(PixelSize size) const;
    Image Thumbnail(Pixel width) const;

    Pixel Width() const;
    Pixel Height() const;
    PixelSize Size() const;
    PixelDensity Density(::Size real_size) const;

    uint64_t Hash() const;

    void DebugDisplay() const;

  private:
    void Release();

    VipsImage* m_Impl{};
};
