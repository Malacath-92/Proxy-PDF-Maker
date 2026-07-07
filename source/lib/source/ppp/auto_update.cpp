#include <ppp/auto_update.hpp>

#include <filesystem>
#include <functional>
#include <future>
#include <ranges>
#include <span>
#include <string_view>
#include <thread>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QRunnable>
#include <QThreadPool>

#include <fmt/format.h>

#include <archive.h>
#include <archive_entry.h>

#include <whereami.h>

#include <ppp/github_request.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

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

const char c_UpdateFolder[]{ "_proxy_pdf_update" };
const char c_BackupFolder[]{ "_proxy_pdf_backup" };

#define AUTO_UPDATE_ARG_START "--update-"
constexpr char c_AutoUpdateArgStart[]{ AUTO_UPDATE_ARG_START };
constexpr char c_AutoUpdateBackupOldVersion[]{ AUTO_UPDATE_ARG_START "backup" };
constexpr char c_AutoUpdateCopyNewVersion[]{ AUTO_UPDATE_ARG_START "execute" };
constexpr char c_AutoUpdateCleanup[]{ AUTO_UPDATE_ARG_START "cleanup" };
#undef AUTO_UPDATE_ARG_START

class UnzipWorker : public QObject, public QRunnable
{
    Q_OBJECT

  public:
    UnzipWorker(QByteArray archive_data,
                fs::path output_folder)
        : m_ArchiveData{ std::move(archive_data) }
        , m_OutputFolder{ std::move(output_folder) }
    {
    }

    virtual void run() override
    {
        if (m_ArchiveData.isEmpty())
        {
            Failed();
            return;
        }

        if (!fs::exists(m_OutputFolder))
        {
            fs::create_directories(m_OutputFolder);
        }

        auto* reader{ archive_read_new() };
        AtScopeExit reader_deleter{ std::bind_front(archive_read_free, reader) };
        auto* writer{ archive_write_disk_new() };
        AtScopeExit writer_deleter{ std::bind_front(archive_write_free, writer) };

        if (reader == nullptr || writer == nullptr)
        {
            Failed();
            return;
        }

        // Enable formats (.zip and .tar) and filters (.gz compression)
        archive_read_support_format_tar(reader);
        archive_read_support_format_zip(reader);
        archive_read_support_filter_gzip(reader);

        static constexpr int c_Flags{
            ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL
        };
        archive_write_disk_set_options(writer, c_Flags);
        archive_write_disk_set_standard_lookup(writer);

        if (archive_read_open_memory(reader, m_ArchiveData.data(), m_ArchiveData.size()) != ARCHIVE_OK)
        {
            Failed();
            return;
        }

        archive_entry* entry;
        while (archive_read_next_header(reader, &entry) == ARCHIVE_OK)
        {
            const auto current_path{ archive_entry_pathname(entry) };
            const auto full_output_path{ m_OutputFolder / current_path };
            const auto full_output_path_str{ full_output_path.string() };
            archive_entry_set_pathname(entry, full_output_path_str.c_str());

            const auto write_header_res{ archive_write_header(writer, entry) };
            if (write_header_res < ARCHIVE_OK)
            {
                Failed("Failed writing file header while extracting file from archive.");
                return;
            }

            // Extract the file content by streaming data blocks from reader to writer
            if (archive_entry_size(entry) > 0)
            {
                const void* buff;
                size_t size;
                int64_t offset;

                while (true)
                {
                    const auto read_block_res{
                        archive_read_data_block(reader, &buff, &size, &offset)
                    };
                    if (read_block_res == ARCHIVE_EOF)
                    {
                        break;
                    }
                    if (read_block_res < ARCHIVE_OK)
                    {
                        Failed("Failed reading data from archive.");
                        return;
                    }

                    const auto write_block_res{
                        archive_write_data_block(writer, buff, size, offset)
                    };
                    if (write_block_res < ARCHIVE_OK)
                    {
                        Failed("Failed writing data from archive to disk.");
                        break;
                    }
                }
            }

            archive_write_finish_entry(writer);
        }

        Succeeded();
    }

    enum class Conclusion
    {
        Pending,
        Failed,
        Success,
    };
    Conclusion GetConclusion() const
    {
        return m_Conclusion;
    }
    bool HasError() const
    {
        return !m_Error.empty();
    }
    const std::string& GetError() const
    {
        return m_Error;
    }

  signals:
    void Done();

  private:
    void Failed()
    {
        m_Conclusion = Conclusion::Failed;
        Done();
    }
    void Failed(std::string error)
    {
        m_Conclusion = Conclusion::Failed;
        m_Error = std::move(error);
        Done();
    }
    void Succeeded()
    {
        m_Conclusion = Conclusion::Success;
        Done();
    }

    QByteArray m_ArchiveData;
    fs::path m_OutputFolder;
    Conclusion m_Conclusion = Conclusion::Pending;
    std::string m_Error;
};

bool AutoUpdateDownloadRelease(std::string_view version)
{
    QNetworkAccessManager network_manager;

    const auto release_url{ fmt::format("https://api.github.com/repos/Malacath-92/Proxy-PDF-Maker/releases/tags/{}", version) };
    QNetworkRequest release_json_request{ PrepareGithubRequest(ToQString(release_url)) };
    QNetworkReply* release_json_reply{ network_manager.get(std::move(release_json_request)) };

    {
        QEventLoop loop;
        QObject::connect(release_json_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    AtScopeExit delete_json_reply{
        [release_json_reply]()
        { delete release_json_reply; }
    };

    if (release_json_reply->error() != QNetworkReply::NetworkError::NoError)
    {
        LogWarning("Failed fetching release json: {}",
                   QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(release_json_reply->error()));
        return false;
    }

    const auto reply_json{ QJsonDocument::fromJson(release_json_reply->readAll()) };
    if (reply_json.isEmpty())
    {
        LogWarning("Empty reply for release json.");
        return false;
    }

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM "windows_"
#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM "osx_"
#elif defined(__linux__)
#define PLATFORM "ubunutu_"
#else
#error "Unknown Platform"
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64)
#define ARCH "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH "arm64"
#else
#error "Unknown Arch"
#endif

#if defined(__linux__)
#define ZIP ".tar.gz"
#else
#define ZIP ".zip"
#endif

    static constexpr char c_AssetSuffix[]{
        PLATFORM ARCH ZIP
    };

    const auto asset_url{
        [&]() -> std::optional<QString>
        {
            const auto assets(reply_json["assets"].toArray());
            for (const auto& asset_elem : assets)
            {
                const auto asset_obj{ asset_elem.toObject() };
                const auto asset_name{ asset_obj["name"].toString() };
                if (asset_name.endsWith(c_AssetSuffix))
                {
                    return asset_obj["browser_download_url"].toString();
                }
            }

            return std::nullopt;
        }()
    };

    if (!asset_url.has_value())
    {
        LogError("No asset ending with {}",
                 PLATFORM ARCH ZIP);
        return false;
    }

    QNetworkRequest release_data_request{ PrepareGithubRequest(asset_url.value()) };
    QNetworkReply* release_data_reply{ network_manager.get(std::move(release_data_request)) };

    {
        QEventLoop loop;
        QObject::connect(release_data_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    AtScopeExit delete_data_reply{
        [release_data_reply]()
        { delete release_data_reply; }
    };

    if (release_data_reply->error() != QNetworkReply::NetworkError::NoError)
    {
        LogWarning("Failed fetching release data: {}",
                   QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(release_json_reply->error()));
        return false;
    }

    auto release_data{ release_data_reply->readAll() };
    if (release_data.isEmpty())
    {
        LogWarning("Empty reply for release data.");
        return false;
    }

    auto* unzip_worker{ new UnzipWorker{ std::move(release_data), c_UpdateFolder } };
    unzip_worker->setAutoDelete(false);
    QThreadPool::globalInstance()->start(unzip_worker, 100);

    {
        QEventLoop loop;
        QObject::connect(unzip_worker, &UnzipWorker::Done, &loop, &QEventLoop::quit);
        loop.exec();
    }

    unzip_worker->deleteLater();
    switch (unzip_worker->GetConclusion())
    {
    case UnzipWorker::Conclusion::Success:
        return true;
    default:
        if (unzip_worker->HasError())
        {
            LogFatal("Unzip Error: {}", unzip_worker->GetError());
        }
        return false;
    }
}

void Execute(QString process, std::span<char*> argv, QStringList extra_args = {})
{
    for (auto* arg : argv)
    {
        extra_args.append(arg);
    }
    LogInfo("Executing detached subprocess: {} {}",
            process.toStdString(),
            extra_args.join(" ").toStdString());
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

    QFile manifest_in{ c_InstallManifestBundled };
    if (manifest_in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QFile manifest_out{ target_manifest };
        if (manifest_out.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            manifest_out.write(manifest_in.readAll());
        }
    }
    else
    {
        LogFatal("Failed opening bundled install manifest...");
    }

    return true;
}

AutoUpdateConclusion AutoUpdateTryInitialize(std::span<char*> argv)
{
    // Trigger auto-update if the library wrote the update files
    const auto update_folder{ c_ExeDir / c_UpdateFolder };
    if (fs::exists(update_folder) && std::filesystem::is_directory(update_folder))
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

        // Execute the new executable to backup the old version
        Execute(
            ToQString(update_folder / c_ExeName),
            argv | std::views::drop(1),
            {
                c_AutoUpdateBackupOldVersion,                     // move
                ToQString(c_ExeDir),                              // path
                ToQString(c_ExeDir / c_InstallManifestExtracted), // manifest
            });

        return AutoUpdateConclusion::Initiated;
    }

    return AutoUpdateConclusion::NoUpdate;
}

void MoveFileWithRetryStrategy(const fs::path& from, const fs::path& to)
{
    static constexpr auto c_MaxRetries{ 5 };
    static constexpr std::chrono::milliseconds c_InitialDelay{ 500 };

    if (!fs::exists(from))
    {
        return;
    }

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
            LogFatal("Can't move file {}...", from.string());
        }
    }

    LogFatal("Can't move file {} after retries...", from.string());
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
        LogFatal("Failed reading old install manifest...");
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
        MoveFileWithRetryStrategy(from / installed_file.toStdString(),
                                  to / installed_file.toStdString());
    }

    // Delete old version's manifest
    fs::remove(manifest);

    // Write out new version's install manifest
    if (!ExtractInstallManifest(from))
    {
        LogFatal("Failed writing old install manifest...");
    }

    // Execute the backed-up executable to move the new executable
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
        MoveFileWithRetryStrategy(from / installed_file.toStdString(),
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

#include "auto_update.moc"
