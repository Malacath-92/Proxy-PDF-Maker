#include <ppp/auto_update.hpp>

#include <filesystem>
#include <ranges>
#include <span>
#include <string_view>
#include <thread>

#include <QCoreApplication>
#include <QFile>
#include <QProcess>

#include <whereami.h>

namespace fs = std::filesystem;

const auto c_GetExePath{
    []()
    {
        static const auto exe_path{
            []
            {
                char exe_path[2048]{};
                wai_getExecutablePath(exe_path, sizeof(exe_path), nullptr);
                return fs::path{ exe_path };
            }()
        };
        return exe_path;
    }
};
const auto c_ExeDir{ c_GetExePath().parent_path() };
const auto c_ExeName{ c_GetExePath().filename() };
const char c_InstallManifestBundled[]{ ":/install_manifest.txt" };
const char c_InstallManifestExtracted[]{ "./install_manifest.txt" };

// Must match the folder that the library writes to
const auto c_UpdateFolder{ "_proxy_pdf_update" };
const auto c_BackupFolder{ "_proxy_pdf_backup" };

#define AUTO_UPDATE_ARG_START "--update-"
constexpr char c_AutoUpdateArgStart[]{ AUTO_UPDATE_ARG_START };
constexpr char c_AutoUpdateBackupOldVersion[]{ AUTO_UPDATE_ARG_START "backup" };
constexpr char c_AutoUpdateCopyNewVersion[]{ AUTO_UPDATE_ARG_START "execute" };
constexpr char c_AutoUpdateCleanup[]{ AUTO_UPDATE_ARG_START "cleanup" };
#undef AUTO_UPDATE_ARG_START

QString ToQString(const fs::path& path)
{
    return QString::fromWCharArray(path.c_str());
}

void Execute(QString process, std::span<char*> argv, QStringList extra_args = {})
{
    for (auto* arg : argv)
    {
        extra_args.append(arg);
    }
    QProcess::startDetached(process, extra_args);
}

bool ExtractInstallManifest(const fs::path& dir)
{
    if (!fs::exists(dir) && !fs::is_directory(dir))
    {
        return false;
    }

    const auto target_manifest{ dir / c_InstallManifestExtracted };
    if (fs::exists(target_manifest))
    {
        fs::remove(target_manifest);
    }

    Q_INIT_RESOURCE(resources);

    QFile manifest_in{ c_InstallManifestBundled };
    if (manifest_in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QFile manifest_out{ target_manifest };
        if (manifest_out.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            manifest_out.write(manifest_in.readAll());
        }
    }
}

AutoUpdateConclusion AutoUpdateTryInitialize(std::span<char*> argv)
{
    // Trigger auto-update if the library wrote the update files
    const auto update_folder{ c_ExeDir / c_UpdateFolder };
    if (fs::exists(update_folder) && std::filesystem::is_directory(update_folder))
    {
        try
        {
            if (fs::exists(c_ExeDir / c_BackupFolder))
            {
                fs::remove_all(c_ExeDir / c_BackupFolder);
            }

            // Write out old version's install manifest
            if (!ExtractInstallManifest(c_ExeDir))
            {
                return AutoUpdateConclusion::Error;
            }

            // Execute the new launcher to backup the old version
            Execute(
                ToQString(update_folder / c_ExeName),
                argv | std::views::drop(1),
                {
                    c_AutoUpdateBackupOldVersion,                     // move
                    ToQString(c_ExeDir),                              // path
                    ToQString(c_ExeDir / c_InstallManifestExtracted), // manifest
                });
        }
        catch (...)
        {
            return AutoUpdateConclusion::Error;
        }
    }

    return AutoUpdateConclusion::NoUpdate;
}

void MoveFile(const fs::path& from, const fs::path& to)
{
    static constexpr auto c_MaxRetries{ 5 };
    static constexpr std::chrono::milliseconds c_InitialDelay{ 500 };

    auto attempts{ 0 };
    std::chrono::milliseconds delay{ c_InitialDelay };

    while (attempts < c_MaxRetries)
    {
        std::error_code ec;
        fs::rename(from, to, ec);

        if (!ec)
        {
            return;
        }

        if (ec == std::errc::permission_denied ||
            ec == std::errc::device_or_resource_busy)
        {
            attempts++;

            std::this_thread::sleep_for(delay);
            delay *= 2; // Exponential backoff
        }
        else
        {
            throw std::logic_error{ "Can't move file..." };
        }
    }

    throw std::logic_error{ "Can't move file after retries..." };
}

QStringList ExtractManifestContents(const fs::path& manifest_path)
{
    QStringList manifest_out{};
    QFile manifest_in{ manifest_path };
    if (manifest_in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream manifest_stream{ &manifest_in };
        while (!manifest_stream.atEnd())
        {
            manifest_out.push_back(manifest_stream.readLine());
        }
    }
    else
    {
        throw std::logic_error{ "Failed reading old install manifest..." };
    }
    return manifest_out;
}

bool AutoUpdateBackupOldVersion(std::span<char*> argv)
{
    const fs::path from{ argv[2] };
    const fs::path manifest{ argv[3] };
    const auto manifest_contents{ ExtractManifestContents(manifest) };

    // Move all declared files
    const auto to{ from / c_BackupFolder };
    if (!fs::exists(to))
    {
        fs::create_directories(to);
    }
    for (const auto& installed_file : manifest_contents)
    {
        MoveFile(from / installed_file.toStdString(),
                 to / installed_file.toStdString());
    }

    // Delete old version's manifest
    fs::remove(manifest);

    // Write out new version's install manifest
    if (!ExtractInstallManifest(from))
    {
        throw std::logic_error{ "Failed writing old install manifest..." };
    }

    // Execute the backed-up launcher to move the new launcher
    Execute(ToQString(c_BackupFolder / c_ExeName),
            argv | std::views::drop(4),
            {
                c_AutoUpdateCopyNewVersion,                   // move
                ToQString(from / c_UpdateFolder),             // from
                ToQString(from),                              // to
                ToQString(from / c_InstallManifestExtracted), // manifest
            });

    return true;
}

bool AutoUpdateUpdateCopyNewVersion(std::span<char*> argv)
{
    const fs::path from{ argv[2] };
    const fs::path to{ argv[3] };
    const fs::path manifest{ argv[4] };
    const auto manifest_contents{ ExtractManifestContents(manifest) };

    // Move all declared files
    if (!fs::exists(to))
    {
        fs::create_directories(to);
    }
    for (const auto& installed_file : manifest_contents)
    {
        MoveFile(from / installed_file.toStdString(),
                 to / installed_file.toStdString());
    }

    // Delete new version's manifest
    fs::remove(manifest);

    // Execute the cleanup, deleting the update folder
    Execute(ToQString(to / c_ExeName),
            argv | std::views::drop(5),
            {
                c_AutoUpdateCleanup,
                ToQString(from),
            });

    return true;
}

bool AutoUpdateCleanup(std::span<char*> argv)
{
    const auto update_folder{ argv[2] };
    if (fs::exists(update_folder))
    {
        fs::remove_all(update_folder);
    }

    return false;
}

AutoUpdatePhase* ResolveAutoUpdatePhase(std::span<char*> argv)
{
    for (const std::string_view arg : argv | std::views::drop(1))
    {
        if (arg.starts_with(c_AutoUpdateArgStart))
        {
            if (arg == c_AutoUpdateBackupOldVersion)
            {
                return &AutoUpdateBackupOldVersion;
            }
            else if (arg == c_AutoUpdateCopyNewVersion)
            {
                return &AutoUpdateUpdateCopyNewVersion;
            }
            else if (arg == c_AutoUpdateCleanup)
            {
                return &AutoUpdateCleanup;
            }
        }
    }

    return nullptr;
}
