#include <ppp/version_check.hpp>

#include <string_view>

#include <fmt/format.h>

#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>
#include <ppp/version.hpp>

inline constexpr std::string_view c_LatestReleaesURI{
    "https://api.github.com/repos/Malacath-92/Proxy-PDF-Maker/releases/latest"
};

std::optional<std::string> NewAvailableVersion()
{
    QNetworkAccessManager network_manager;

    QNetworkRequest request{ ToQString(c_LatestReleaesURI) };
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
        // Failed fetching latest release
        return std::nullopt;
    }

    const auto reply_json{ QJsonDocument::fromJson(reply->readAll()) };
    if (reply_json.isEmpty())
    {
        // Empty reply
        return std::nullopt;
    }

    const auto latest_version{ reply_json["tag_name"].toString().toStdString() };
    const auto this_version{ ProxyPdfVersion() };

    const auto latest_semver{ ProxyPdfToSemanticVersion(latest_version) };
    const auto this_semver{ ProxyPdfToSemanticVersion(this_version) };

    if (latest_semver <= this_semver)
    {
        return std::nullopt;
    }

    return latest_version;
}

std::string ReleaseURL(std::string_view version)
{
    return fmt::format("https://github.com/Malacath-92/Proxy-PDF-Maker/releases/tag/{}", version);
}
