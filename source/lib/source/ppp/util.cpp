#include <ppp/util.hpp>

#include <ppp/qt_util.hpp>

#include <ranges>

#include <QDesktopServices>
#include <QString>
#include <QUrl>

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
    return QDesktopServices::openUrl(QUrl("file:///" + ToQString(path), QUrl::TolerantMode));
}
