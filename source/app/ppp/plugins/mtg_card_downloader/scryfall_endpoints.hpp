#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <optional>

#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class ScryfallEndpoint : public QObject
{
    Q_OBJECT

  public:
    ScryfallEndpoint(QNetworkAccessManager& network_manager,
                     QString endpoint_name,
                     std::chrono::milliseconds rate_limit);
    virtual ~ScryfallEndpoint();

  signals:
    void OnError();

  protected:
    using OnDoneFun = std::function<void(QNetworkReply*)>;
    void QueueRequest(QString request_uri, OnDoneFun on_done, QJsonDocument body = {});
    void QueueRequest(QNetworkRequest request, OnDoneFun on_done, QByteArray body = {});

  private:
    void DoRequest(QNetworkRequest request, OnDoneFun on_done, QByteArray body);
    void Available();

    QNetworkAccessManager& m_NetworkManager;
    QString m_EndpointName;
    std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> m_LastCall;
    std::optional<QTimer> m_RateLimiter;

    struct QueuedRequest
    {
        QNetworkRequest m_Request;
        OnDoneFun m_OnDone;
        QByteArray m_Body;
    };

    std::deque<QueuedRequest> m_RequestQueue;
};

class ScryfallSearchEndpoint : public ScryfallEndpoint
{
    Q_OBJECT

  public:
    ScryfallSearchEndpoint(QNetworkAccessManager& network_manager);

    using OnDoneFun = std::function<void(const QJsonDocument&)>;
    void Queue(const QString& query, OnDoneFun on_done);

  private:
    void HandleReply(QNetworkReply* reply);

    void DoRequest();

    struct Query
    {
        QString m_Query;
        OnDoneFun m_OnDone;

        std::optional<QJsonDocument> m_AccumulatedJson;
    };
    std::deque<Query> m_Queue;
};

class ScryfallCollectionEndpoint : public ScryfallEndpoint
{
    Q_OBJECT

  public:
    ScryfallCollectionEndpoint(QNetworkAccessManager& network_manager);

    inline static constexpr size_t c_BatchSize{ 75 };

    using OnDoneFun = std::function<void(const QJsonDocument& batch, const QJsonDocument& result)>;
    void Queue(QJsonDocument batch, OnDoneFun on_done);
};

class ScryfallDataEndpoint : public ScryfallEndpoint
{
    Q_OBJECT

  public:
    ScryfallDataEndpoint(QNetworkAccessManager& network_manager);

    using OnDoneFun = std::function<void(const QByteArray&)>;
    void Call(const QString& uri, OnDoneFun on_done);
};
