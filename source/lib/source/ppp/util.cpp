#include <ppp/util.hpp>

#include <ppp/qt_util.hpp>

#include <charconv>
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
        return fmt::format("{}1{}", base_name.string(), base_ext.string());
    }
    else
    {
        return fmt::format("{}{}{}", base_name.string(), versions.back() + 1, base_ext.string());
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
