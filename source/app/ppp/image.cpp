#include <ppp/image.hpp>

#include <opencv2/imgcodecs.hpp>

#include <ppp/util.hpp>

inline const std::array ValidImageExtensions{
    ".bmp"_p,
    ".gif"_p,
    ".jpg"_p,
    ".jpeg"_p,
    ".png"_p,
};

template<class FunT>
void ForEachImageFile(const fs::path& path, FunT&& fun)
{
    ForEachFile(path, std::forward<FunT>(fun), ValidImageExtensions);
}

std::vector<fs::path> ListImageFiles(const fs::path& path)
{
    return ListFiles(path, ValidImageExtensions);
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
    m_Impl = rhs.m_Impl;
    return *this;
}

void Image::Init(const char* program_name)
{
    (void)program_name;
}

void Image::InitFolders(const fs::path& image_dir, const fs::path& crop_dir)
{
    for (const auto& folder : { image_dir, crop_dir })
    {
        if (!std::filesystem::exists(folder))
        {
            std::filesystem::create_directories(folder);
        }
    }
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

void Image::Release()
{
    m_Impl = cv::Mat{};
}
