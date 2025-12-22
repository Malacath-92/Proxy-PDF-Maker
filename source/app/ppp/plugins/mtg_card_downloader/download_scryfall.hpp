#pragma once

#include <optional>

#include <QRegularExpression>
#include <QString>
#include <QTimer>

#include <ppp/plugins/mtg_card_downloader/download_interface.hpp>

class QNetworkAccessManager;
class QNetworkReply;
class QJsonDocument;

struct DecklistCard;

class ScryfallDownloader : public CardArtDownloader
{
    Q_OBJECT

  public:
    ScryfallDownloader(std::vector<QString> skip_files,
                       const std::optional<QString>& backside_pattern);
    virtual ~ScryfallDownloader() override;

    virtual bool ParseInput(const QString& input) override;
    virtual bool BeginDownload(QNetworkAccessManager& network_manager) override;
    virtual void HandleReply(QNetworkReply* reply) override;

    virtual std::vector<QString> GetFiles() const override;

    virtual uint32_t GetAmount(const QString& file_name) const override;
    virtual std::optional<QString> GetBackside(const QString& file_name) const override;
    virtual std::vector<QString> GetDuplicates(const QString& file_name) const override;

    virtual bool ProvidesBleedEdge() const override;

  private:
    struct BacksideRequest;

    static bool HasBackside(const QJsonDocument& card_info);
    static QString CardBackFilename(const QString& card_back_id);

    enum class RequestType
    {
        CardInfo,
        CardImage,
        BacksideImage,
    };

    QNetworkReply* DoRequestWithMetadata(QString request_uri, QString file_name, size_t index, RequestType request_type);
    QNetworkReply* DoRequestWithoutMetadata(QString request_uri);

    bool NextRequest();

    std::vector<QString> m_Queries;
    std::optional<QString> m_QueryMoreData;

    std::vector<DecklistCard> m_Cards;
    std::unordered_map<QString, size_t> m_FileNameIndexMap;

    std::vector<QJsonDocument> m_CardInfos;
    std::vector<BacksideRequest> m_Backsides;
    uint32_t m_Downloads{ 0 };

    uint32_t m_Progress{ 0 };

    struct MetaData
    {
        QString m_FileName;
        size_t m_Index;
        RequestType m_Type;
    };
    std::unordered_map<const QNetworkReply*, MetaData> m_ReplyMetaData;

    size_t m_TotalRequests{};
    std::vector<QNetworkReply*> m_Requests{};
    QTimer m_ScryfallTimer;
    QNetworkAccessManager* m_NetworkManager{ nullptr };
};
