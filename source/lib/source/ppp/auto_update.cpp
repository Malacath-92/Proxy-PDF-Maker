#include <ppp/auto_update.hpp>

#include <future>

#include <fmt/format.h>

#include <archive.h>
#include <archive_entry.h>

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRunnable>
#include <QThreadPool>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

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
        AtScopeExit reader_deleter{ std::bind_back(archive_read_free, reader) };
        auto* writer{ archive_write_disk_new() };
        AtScopeExit writer_deleter{ std::bind_back(archive_write_free, writer) };

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
        int res;
        while (archive_read_next_header(reader, &entry) == ARCHIVE_OK)
        {
            const auto current_path{ archive_entry_pathname(entry) };
            const auto full_output_path{ m_OutputFolder / current_path };
            const auto full_output_path_str{ full_output_path.string() };
            archive_entry_set_pathname(entry, full_output_path_str.c_str());

            res = archive_write_header(writer, entry);
            if (res < ARCHIVE_OK)
            {
                // TODO: Handle error
                continue;
            }

            // Extract the file content by streaming data blocks from reader to writer
            if (archive_entry_size(entry) > 0)
            {
                const void* buff;
                size_t size;
                int64_t offset;

                while (true)
                {
                    res = archive_read_data_block(reader, &buff, &size, &offset);
                    if (res == ARCHIVE_EOF)
                    {
                        break;
                    }
                    if (res < ARCHIVE_OK)
                    {
                        // TODO: Handle error
                        break;
                    }

                    res = archive_write_data_block(writer, buff, size, offset);
                    if (res < ARCHIVE_OK)
                    {
                        // TODO: Handle error
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

  signals:
    void Done();

  private:
    void Failed()
    {
        m_Conclusion = Conclusion::Failed;
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
};

bool DownloadRelease(std::string_view version)
{
    QNetworkAccessManager network_manager;

    const auto release_url{ fmt::format("https://api.github.com/repos/Malacath-92/Proxy-PDF-Maker/releases/tags/{}", version) };
    QNetworkRequest release_json_request{ ToQString(release_url) };
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
        // Failed fetching release json
        return false;
    }

    const auto reply_json{ QJsonDocument::fromJson(release_json_reply->readAll()) };
    if (reply_json.isEmpty())
    {
        // Empty reply
        return false;
    }

    static constexpr char c_AssetSuffix[]{
#if defined(_WIN32) || defined(_WIN64)
        "windows_"
#elif defined(__APPLE__) && defined(__MACH__)
        "osx_"
#elif defined(__linux__)
        "ubunutu_"
#else
#error "Unknown Platform"
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64)
        "amd64"
        //"x86_64"
#elif defined(__i386__) || defined(_M_IX86)
        "arm64"
#else
#error "Unknown Arch"
#endif

#if defined(__linux__)
        ".tar.gz"
#else
        ".zip"
#endif
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
        // No asset for platform + arch
        return false;
    }

    QNetworkRequest release_data_request{ asset_url.value() };
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
        // Failed fetching release data
        return false;
    }

    auto release_data{ release_data_reply->readAll() };
    if (release_data.isEmpty())
    {
        // Empty reply
        return false;
    }

    static constexpr char c_UpdateFolder[]{ "_proxy_pdf_update" };
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
        return false;
    }
}

#include "auto_update.moc"
