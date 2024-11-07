#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include <opencv2/core/mat.hpp>

#include <ppp/util.hpp>

template<class FunT>
void ForEachImageFile(const fs::path& path, FunT&& fun);

std::vector<fs::path> ListImageFiles(const fs::path& path);

class Image
{
  public:
    Image() = default;
    ~Image();

    Image(Image&& rhs);
    Image(const Image& rhs);

    Image& operator=(Image&& rhs);
    Image& operator=(const Image& rhs);

    static void Init(const char* program_name);
    static void InitFolders(const fs::path& image_dir, const fs::path& crop_dir);

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

  private:
    void Release();

    cv::Mat m_Impl{};
};
