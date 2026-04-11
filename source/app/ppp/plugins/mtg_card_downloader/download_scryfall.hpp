#pragma once

#include <optional>
#include <vector>

#include <QString>

#include <ppp/plugins/mtg_card_downloader/download_interface.hpp>

class QNetworkAccessManager;
class QNetworkReply;
class QJsonDocument;

struct DecklistCard;

class ScryfallCollectionEndpoint;
class ScryfallSearchEndpoint;
class ScryfallDataEndpoint;

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

  private slots:
    void OnError(){};

  private:
    static bool HasBackside(const QJsonDocument& card_info);
    static QString CardBackFilename(const QString& card_back_id);

    void RunQueries();
    void DownloadCardDatas();
    void DownloadCardImages();

    std::unique_ptr<ScryfallCollectionEndpoint> m_CollectionEndpoint;
    std::unique_ptr<ScryfallDataEndpoint> m_DataEndpoint;

    std::unique_ptr<ScryfallSearchEndpoint> m_SearchEndpoint;

    std::vector<QString> m_Queries;
    std::optional<QString> m_QueryMoreData;

    std::vector<DecklistCard> m_Cards;
    std::unordered_map<QString, size_t> m_FileNameIndexMap;

    std::vector<QJsonDocument> m_CardInfos;
    std::vector<QString> m_BacksideFiles;

    uint32_t m_Progress{ 0 };

    size_t m_TotalRequests{};
};
