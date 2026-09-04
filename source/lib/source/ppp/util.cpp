#include <ppp/util.hpp>

#include <charconv>
#include <ranges>

#include <QDesktopServices>
#include <QFileInfo>
#include <QStorageInfo>
#include <QString>
#include <QUrl>

#include <whereami.h>

#include <ppp/qt_util.hpp>

size_t CountFiles(const fs::path& path,
                  const std::span<const fs::path> extensions,
                  bool recursive)
{
    size_t num_files{ 0 };
    const auto count_fun{
        [&num_files](const fs::path& /*path*/)
        {
            ++num_files;
        }
    };
    if (!recursive)
    {
        ForEachFile(path, count_fun, extensions);
    }
    else
    {
        ForEachFileRecursive(path, count_fun, extensions);
    }
    return num_files;
}

std::vector<fs::path> ListFiles(const fs::path& path,
                                const std::span<const fs::path> extensions,
                                bool recursive)
{
    std::vector<fs::path> files;
    const auto push_fun{
        [&files](const fs::path& path)
        {
            files.push_back(path.filename());
        }
    };
    if (!recursive)
    {
        ForEachFile(path, push_fun, extensions);
    }
    else
    {
        ForEachFileRecursive(path, push_fun, extensions);
    }
    return files;
}

std::vector<fs::path> ListFolders(const fs::path& path,
                                  bool recursive)
{
    std::vector<fs::path> folders;
    const auto push_fun{
        [&folders](const fs::path& path)
        {
            folders.push_back(path.filename());
        }
    };
    if (!recursive)
    {
        ForEachFolder(path, push_fun);
    }
    else
    {
        ForEachFolderRecursive(path, push_fun);
    }
    return folders;
}

fs::path GetNextVersionedPath(const fs::path& base_path)
{
    const fs::path base_name{ base_path.stem().string() + "_" };
    const auto base_ext{ base_path.extension() };
    const auto parent_path{ fs::absolute(base_path).parent_path() };
    auto pathes{ base_ext.empty()
                     ? ListFolders(parent_path)
                     : ListFiles(parent_path, std::array{ base_ext }) };
    std::ranges::sort(pathes);

    const std::basic_string_view base_name_str{ base_name.c_str() };
    const auto versions{
        pathes |
            std::views::transform(&fs::path::stem) |
            std::views::filter(
                [&](const fs::path& path_stem)
                {
                    const std::basic_string_view path_name{ path_stem.c_str() };
                    return path_name.starts_with(base_name_str) &&
                           (path_name.substr(base_name_str.size()) |
                            std::views::filter([](char c)
                                               { return !std::isdigit(c); }) |
                            std::ranges::to<std::vector>())
                                   .size() == 0;
                }) |
            std::views::transform(
                [&](const fs::path& path_stem)
                {
                    const auto number_str{
                        path_stem
                            .string()
                            .substr(base_name_str.size()),
                    };

                    uint32_t num{};
                    std::from_chars(number_str.data(), number_str.data() + number_str.size(), num);
                    return num;
                }) |
            std::ranges::to<std::vector>(),
    };

    if (versions.empty())
    {
        return parent_path / fmt::format("{}1{}", base_name.string(), base_ext.string());
    }
    else
    {
        return parent_path / fmt::format("{}{}{}", base_name.string(), versions.back() + 1, base_ext.string());
    }
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

bool CanMoveFiles(const fs::path& from, const fs::path& to)
{
    const QStorageInfo from_storage{
        fs::is_directory(from) ? QDir{ from }.absolutePath()
                               : QFileInfo{ from }.absolutePath()
    };
    const QStorageInfo to_storage{
        fs::is_directory(to) ? QDir{ to }.absolutePath()
                             : QFileInfo{ to }.absolutePath()
    };
    return from_storage.rootPath() == to_storage.rootPath();
}
bool SafeMove(const fs::path& from, const fs::path& to)
{
    const bool can_move{ CanMoveFiles(from, to) };
    if (can_move)
    {
        if (fs::is_regular_file(from) && fs::is_directory(to))
        {
            fs::rename(from, to / from.filename());
        }
        else
        {
            fs::rename(from, to);
        }
    }
    else
    {
        fs::copy(from, to);
    }

    return true;
}

fs::path GetExePath()
{
    static const auto s_ExePath{
        []
        {
            char exe_path[2048]{};
            wai_getExecutablePath(exe_path, sizeof(exe_path), nullptr);
            return fs::path{ exe_path };
        }()
    };
    return s_ExePath;
}
