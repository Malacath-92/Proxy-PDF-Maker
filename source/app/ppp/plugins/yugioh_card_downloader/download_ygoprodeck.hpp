#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QTimer>

#include <ppp/plugins/download_interface.hpp>

class QNetworkAccessManager;
class QNetworkReply;
class QJsonDocument;

struct DecklistCard;

class ScryfallCollectionEndpoint;
class ScryfallSearchEndpoint;
class ScryfallDataEndpoint;

class YGOProDeckDownloader : public CardArtDownloader
{
    Q_OBJECT

  public:
    YGOProDeckDownloader(std::vector<QString> skip_files);
    virtual ~YGOProDeckDownloader() override;

    virtual bool ParseInput(const QString& input) override;
    virtual bool BeginDownload(QNetworkAccessManager& network_manager) override;
    virtual void HandleReply(QNetworkReply* reply) override;

    virtual std::vector<QString> GetFiles() const override;

    virtual uint32_t GetAmount(const QString& file_name) const override;
    virtual std::optional<QString> GetBackside(const QString& file_name) const override;
    virtual std::vector<QString> GetDuplicates(const QString& file_name) const override;

    virtual bool ProvidesBleedEdge() const override;

  private slots:
    void OnError(){};

  private:
    void NextRequest();

    QNetworkAccessManager* m_NetworkManager;

    struct CardInfo
    {
        uint32_t m_Amount{ 0 };
        QString m_FileName{ "" };
    };
    std::unordered_map<uint32_t, CardInfo> m_Cards;
    std::unordered_map<QString, uint32_t> m_FileNameIdMap;
    std::vector<uint32_t> m_CardIds;

    QTimer m_Timer;

    uint32_t m_Progress{ 0 };
    size_t m_TotalRequests{};
};
