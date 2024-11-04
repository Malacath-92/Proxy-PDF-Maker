#include <ppp/util.hpp>

#include <ranges>

#include <QDesktopServices>
#include <QUrl>

template<class FunT>
void ForEachFile(const std::filesystem::path& path, FunT&& fun, const std::span<const std::filesystem::path> extensions)
{
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    std::vector<std::filesystem::path> files;
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
void ForEachFolder(const std::filesystem::path& path, FunT&& fun)
{
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    std::vector<std::filesystem::path> files;
    for (auto& child : std::filesystem::directory_iterator(path))
    {
        if (child.is_directory())
        {
            fun(child.path());
        }
    }
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& path, const std::span<const std::filesystem::path> extensions)
{
    std::vector<std::filesystem::path> files;
    ForEachFile(
        path,
        [&files](const std::filesystem::path& path)
        {
            files.push_back(path.filename());
        },
        extensions);
    return files;
}

std::vector<std::filesystem::path> ListFolders(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> folders;
    ForEachFolder(
        path,
        [&folders](const std::filesystem::path& path)
        {
            folders.push_back(path.filename());
        });
    return folders;
}

bool OpenFolder(const std::filesystem::path& path)
{
    return OpenPath(path);
}

bool OpenFile(const std::filesystem::path& path)
{
    return OpenPath(path);
}

bool OpenPath(const std::filesystem::path& path)
{
    return QDesktopServices::openUrl(QUrl("file:///" + QString::fromStdWString(path.c_str()), QUrl::TolerantMode));
}
