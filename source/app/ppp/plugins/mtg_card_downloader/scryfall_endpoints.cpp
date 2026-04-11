#include <ppp/plugins/mtg_card_downloader/scryfall_endpoints.hpp>

#include <QByteArray>
#include <QJsonArray>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

#include <ppp/util/log.hpp>

ScryfallEndpoint::ScryfallEndpoint(QNetworkAccessManager& network_manager,
                                   QString endpoint_name,
                                   std::chrono::milliseconds rate_limit)
    : m_NetworkManager{ network_manager }
    , m_EndpointName{ std::move(endpoint_name) }
{
    if (rate_limit.count() > 0)
    {
        m_RateLimiter.emplace();
        m_RateLimiter.value().setInterval(rate_limit.count());
        m_RateLimiter.value().setSingleShot(true);

        QObject::connect(&m_RateLimiter.value(),
                         &QTimer::timeout,
                         this,
                         &ScryfallEndpoint::Available);
    }
}

ScryfallEndpoint::~ScryfallEndpoint() = default;

void ScryfallEndpoint::QueueRequest(QString request_uri, OnDoneFun on_done, QJsonDocument body)
{
    if (request_uri.isEmpty())
    {
        LogError("Empty request... Download cancelled...");
        return;
    }

    LogInfo("Doing request \"{}\"...", request_uri.toStdString());

    QNetworkRequest request{ std::move(request_uri) };
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                      ToQString(fmt::format("Proxy-PDF-Maker/{}", ProxyPdfVersion())));
    request.setRawHeader("Accept",
                         "*/*");

    if (!body.isNull())
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/json");
    }

    QueueRequest(std::move(request),
                 std::move(on_done),
                 body.isNull() ? QByteArray{} : body.toJson());
}
void ScryfallEndpoint::QueueRequest(QNetworkRequest request, OnDoneFun on_done, QByteArray body)
{

    if (m_RateLimiter.has_value() && m_RateLimiter.value().isActive())
    {
        m_RequestQueue.push_back({ std::move(request), std::move(on_done), std::move(body) });
        return;
    }

    DoRequest(std::move(request), std::move(on_done), std::move(body));
}

void ScryfallEndpoint::DoRequest(QNetworkRequest request, OnDoneFun on_done, QByteArray body)
{
    QNetworkReply* reply{
        body.isEmpty() ? m_NetworkManager.get(std::move(request))
                       : m_NetworkManager.post(std::move(request), std::move(body))
    };

    QObject::connect(reply,
                     &QNetworkReply::errorOccurred,
                     reply,
                     [this, reply](QNetworkReply::NetworkError error)
                     {
                         LogError("Error during request {} {}: {}",
                                  reply->request().url().toEncoded().toStdString(),
                                  reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                                  QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
                         OnError();
                     });
    QObject::connect(reply,
                     &QNetworkReply::finished,
                     this,
                     [reply, on_done = std::move(on_done)]()
                     { on_done(reply); });

    if (m_RateLimiter.has_value())
    {
        if (m_LastCall.has_value())
        {
            const auto now{ std::chrono::high_resolution_clock::now() };
            const auto since_last{ now - m_LastCall.value() };
            const auto ms_since_last{ std::chrono::duration_cast<std::chrono::milliseconds>(since_last).count() };
            m_LastCall = now;

            const auto interval{ m_RateLimiter.value().interval() };
            if (ms_since_last < interval * 3)
            {
                LogInfo("{}, {}ms passed, out of {}ms",
                        m_EndpointName.toStdString(),
                        ms_since_last,
                        interval);
            }
        }
        else
        {
            m_LastCall = std::chrono::high_resolution_clock::now();
        }

        m_RateLimiter.value().start();
    }
}

void ScryfallEndpoint::Available()
{
    if (!m_RequestQueue.empty())
    {
        auto& [request, on_done, body]{ m_RequestQueue.front() };
        DoRequest(std::move(request), std::move(on_done), std::move(body));
        m_RequestQueue.pop_front();
    }
}

ScryfallSearchEndpoint::ScryfallSearchEndpoint(QNetworkAccessManager& network_manager)
    : ScryfallEndpoint{ network_manager, "api.scryfall.com/cards/search", std::chrono::milliseconds{ 500 } }
{
}

void ScryfallSearchEndpoint::Queue(const QString& query, OnDoneFun on_done)
{
    m_Queue.push_back(Query{
        query,
        std::move(on_done),
    });

    if (m_Queue.size() == 1)
    {
        DoRequest();
    }
}

void ScryfallSearchEndpoint::HandleReply(QNetworkReply* reply)
{
    QJsonParseError reply_parse_error{};
    auto reply_json{ QJsonDocument::fromJson(reply->readAll(), &reply_parse_error) };

    if (reply_parse_error.error != QJsonParseError::NoError)
    {
        LogError("Failed parsing returned json data: {}",
                 reply_parse_error.errorString().toStdString());
        OnError();
        return;
    }

    std::optional<QString> next_page{};
    if (reply_json["has_more"].toBool())
    {
        next_page = reply_json["next_page"].toString();
    }

    Query& query{ m_Queue.front() };
    if (query.m_AccumulatedJson.has_value())
    {
        const auto reply_data(reply_json["data"].toArray());
        auto accum_data(query.m_AccumulatedJson.value()["data"].toArray());
        for (const auto& data : reply_data)
        {
            accum_data.append(data);
        }
    }
    else
    {
        query.m_AccumulatedJson = std::move(reply_json);
    }

    if (next_page.has_value())
    {
        LogInfo("Query {} has more data, fetching next page...",
                query.m_Query.toStdString());
        ScryfallEndpoint::QueueRequest(
            next_page.value(),
            std::bind_front(&ScryfallSearchEndpoint::HandleReply, this));
    }
    else
    {
        query.m_OnDone(query.m_AccumulatedJson.value());
        m_Queue.pop_front();

        if (!m_Queue.empty())
        {
            DoRequest();
        }
    }

    reply->deleteLater();
}

void ScryfallSearchEndpoint::DoRequest()
{
    ScryfallEndpoint::QueueRequest(QString("https://api.scryfall.com/cards/search?q=%1")
                                       .arg(QUrl::toPercentEncoding(m_Queue.front().m_Query)),
                                   std::bind_front(&ScryfallSearchEndpoint::HandleReply, this));
}

ScryfallCollectionEndpoint::ScryfallCollectionEndpoint(QNetworkAccessManager& network_manager)
    : ScryfallEndpoint{ network_manager, "api.scryfall.com/cards/collection", std::chrono::milliseconds{ 500 } }
{
}

void ScryfallCollectionEndpoint::Queue(QJsonDocument batch, OnDoneFun on_done)
{
    ScryfallEndpoint::QueueRequest(
        "https://api.scryfall.com/cards/collection",
        [batch, on_done = std::move(on_done)](QNetworkReply* reply)
        { on_done(batch, QJsonDocument::fromJson(reply->readAll())); },
        batch);
}

ScryfallDataEndpoint::ScryfallDataEndpoint(QNetworkAccessManager& network_manager)
    : ScryfallEndpoint{ network_manager, "*.scryfall.io", std::chrono::milliseconds{ 0 } }
{
}

void ScryfallDataEndpoint::Call(const QString& uri, OnDoneFun on_done)
{
    ScryfallEndpoint::QueueRequest(
        uri,
        [on_done = std::move(on_done)](QNetworkReply* reply)
        { on_done(reply->readAll()); });
}
