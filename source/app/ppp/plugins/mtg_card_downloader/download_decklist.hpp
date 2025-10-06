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
    ScryfallDownloader(std::vector<QString> skip_files, const QString& backside_pattern);
    virtual ~ScryfallDownloader() override;

    virtual bool ParseInput(const QString& decklist) override;
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

    bool NextRequest();

    std::vector<DecklistCard> m_Cards;
    std::unordered_map<QString, size_t> m_FileNameIndexMap;

    std::vector<QJsonDocument> m_CardInfos;
    std::vector<BacksideRequest> m_Backsides;
    uint32_t m_Downloads{ 0 };

    size_t m_TotalRequests{};
    std::vector<QNetworkReply*> m_Requests{};
    QTimer m_ScryfallTimer;
    QNetworkAccessManager* m_NetworkManager{ nullptr };
};
