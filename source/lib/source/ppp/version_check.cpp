#include <ppp/version_check.hpp>

#include <string_view>

#include <fmt/format.h>

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <ppp/github_request.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

inline constexpr std::string_view c_AllReleasesURI{
    "https://api.github.com/repos/Malacath-92/Proxy-PDF-Maker/releases"
};
inline constexpr std::string_view c_LatestReleaseURI{
    "https://api.github.com/repos/Malacath-92/Proxy-PDF-Maker/releases/latest"
};

std::optional<QJsonObject> GetLatestPreRelease()
{
    QNetworkAccessManager network_manager;

    QNetworkRequest request{ PrepareGithubRequest(ToQString(c_AllReleasesURI)) };
    QNetworkReply* reply{ network_manager.get(std::move(request)) };

    {
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    AtScopeExit delete_reply{
        [reply]()
        { delete reply; }
    };

    if (reply->error() != QNetworkReply::NetworkError::NoError)
    {
        LogWarning("Failed fetching all releases: {}",
                   QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error()));
        return std::nullopt;
    }

    const auto reply_json{ QJsonDocument::fromJson(reply->readAll()) };
    if (reply_json.isEmpty())
    {
        LogWarning("Empty reply for all releases.");
        return std::nullopt;
    }

    if (!reply_json.isArray())
    {
        LogWarning("All releases did not return an array.");
        return std::nullopt;
    }

    QJsonArray releases = reply_json.array();
    for (const QJsonValue& value : releases)
    {
        QJsonObject release = value.toObject();
        if (release.value("prerelease").toBool())
        {
            return release;
        }
    }

    LogWarning("Did not find any pre-release.");
    return std::nullopt;
}

std::optional<QJsonDocument> GetLatestRelease()
{
    QNetworkAccessManager network_manager;

    QNetworkRequest request{ PrepareGithubRequest(ToQString(c_LatestReleaseURI)) };
    QNetworkReply* reply{ network_manager.get(std::move(request)) };

    {
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    AtScopeExit delete_reply{
        [reply]()
        { delete reply; }
    };

    if (reply->error() != QNetworkReply::NetworkError::NoError)
    {
        LogWarning("Failed fetching latest release: {}",
                   QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(reply->error()));
        return std::nullopt;
    }

    const auto reply_json{ QJsonDocument::fromJson(reply->readAll()) };
    if (reply_json.isEmpty())
    {
        LogWarning("Empty reply for latest release.");
        return std::nullopt;
    }

    return reply_json;
}

std::optional<std::string> NewAvailableVersion()
{
    const auto this_version{ ProxyPdfVersion() };
    const auto this_semver{ ProxyPdfToSemanticVersion(this_version) };
    const auto is_nightly{ this_semver == SemanticVersion{ 0, 0, 0 } };

    if (is_nightly)
    {
        LogInfo("This release is a Nightly release, looking for newer Nightly.");
        if (const auto latest_pre_release{ GetLatestPreRelease() })
        {
            const auto this_created_at{ ToQString(ProxyPdfBuildTime()) };
            const auto latest_created_at{ latest_pre_release.value().value("created_at").toString() };

            const auto this_time{ QDateTime::fromString(this_created_at, Qt::ISODate) };
            const auto latest_time{ QDateTime::fromString(latest_created_at, Qt::ISODate) };

            if (!this_time.isValid())
            {
                LogWarning("Could not parse local build time...");
                return std::nullopt;
            }
            if (!latest_time.isValid())
            {
                LogWarning("Could not pre-release build time...");
                return std::nullopt;
            }

            if (latest_time.toMSecsSinceEpoch() <= this_time.toMSecsSinceEpoch())
            {
                LogWarning("Latest pre-release is not newer that this release...");
                return std::nullopt;
            }

            return latest_pre_release.value().value("tag_name").toString().toStdString();
        }
        return std::nullopt;
    }

    if (const auto latest_release{ GetLatestRelease() })
    {
        const auto latest_version{ latest_release.value()["tag_name"].toString().toStdString() };
        const auto latest_semver{ ProxyPdfToSemanticVersion(latest_version) };

        if (latest_semver <= this_semver)
        {
            LogInfo("Latest release is not newer than current: {} <= {}",
                    latest_version,
                    this_version);
            return std::nullopt;
        }

        return latest_version;
    }

    return std::nullopt;
}

std::string ReleaseURL(std::string_view version)
{
    return fmt::format("https://github.com/Malacath-92/Proxy-PDF-Maker/releases/tag/{}", version);
}
