#pragma once

#include <QNetworkRequest>

#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

inline QNetworkRequest PrepareGithubRequest(QString uri)
{
    QNetworkRequest request{ uri };
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                      QString{ "Proxy-PDF-Maker/%1" }.arg(ToQString(ProxyPdfVersion())));
    request.setRawHeader("Accept",
                         "*/*");

    if (qEnvironmentVariableIsSet("GITHUB_API_TOKEN"))
    {
        const QString token{ qEnvironmentVariable("GITHUB_API_TOKEN") };
        const auto auth{ QString{ "Bearer %1" }.arg(token) };
        request.setRawHeader("Authorization",
                             auth.toUtf8());
    }

    return request;
}
