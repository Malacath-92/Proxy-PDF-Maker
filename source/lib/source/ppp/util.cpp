#include <ppp/util.hpp>

#include <ranges>

#include <QDesktopServices>
#include <QUrl>

template<class FunT>
void ForEachFile(const fs::path& path, FunT&& fun, const std::span<const fs::path> extensions)
{
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    std::vector<fs::path> files;
    for (auto& child : std::filesystem::directory_iterator(path))
    {
        if (!child.is_directory())
        {
            const bool is_matching_extension{
                extensions.empty() || std::ranges::contains(extensions, child.path().extension())
            };
            if (is_matching_extension)
            {
                fun(child.path());
            }
        }
    }
}

template<class FunT>
void ForEachFolder(const fs::path& path, FunT&& fun)
{
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    std::vector<fs::path> files;
    for (auto& child : std::filesystem::directory_iterator(path))
    {
        if (child.is_directory())
        {
            fun(child.path());
        }
    }
}

std::vector<fs::path> ListFiles(const fs::path& path, const std::span<const fs::path> extensions)
{
    std::vector<fs::path> files;
    ForEachFile(
        path,
        [&files](const fs::path& path)
        {
            files.push_back(path.filename());
        },
        extensions);
    return files;
}

std::vector<fs::path> ListFolders(const fs::path& path)
{
    std::vector<fs::path> folders;
    ForEachFolder(
        path,
        [&folders](const fs::path& path)
        {
            folders.push_back(path.filename());
        });
    return folders;
}

bool OpenFolder(const fs::path& path)
{
    return OpenPath(fs::absolute(path));
}

bool OpenFile(const fs::path& path)
{
    return OpenPath(fs::absolute(path));
}

bool OpenPath(const fs::path& path)
{
    return QDesktopServices::openUrl(QUrl("file:///" + QString::fromStdWString(path.c_str()), QUrl::TolerantMode));
}
