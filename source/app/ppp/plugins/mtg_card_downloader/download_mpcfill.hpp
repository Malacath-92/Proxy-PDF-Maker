#pragma once

#include <optional>

#include <ppp/plugins/mtg_card_downloader/download_interface.hpp>

class QDomElement;

class MPCFillDownloader : public CardArtDownloader
{
    Q_OBJECT

  public:
    MPCFillDownloader(std::vector<QString> skip_files,
                      const std::optional<QString>& backside_pattern);

    virtual bool ParseInput(const QString& xml) override;
    virtual bool BeginDownload(QNetworkAccessManager& network_manager) override;
    virtual void HandleReply(QNetworkReply* reply) override;

    virtual std::vector<QString> GetFiles() const override;

    virtual uint32_t GetAmount(const QString& file_name) const override;
    virtual std::optional<QString> GetBackside(const QString& file_name) const override;
    virtual std::vector<QString> GetDuplicates(const QString& file_name) const override;

    virtual bool ProvidesBleedEdge() const override;

  private:
    struct MPCFillBackside
    {
        QString m_Name;
        QString m_Id;
    };
    struct MPCFillCard
    {
        QString m_Name;
        QString m_Id;
        uint32_t m_Amount;

        std::optional<MPCFillBackside> m_Backside;
    };
    struct MPCFillSet
    {
        std::vector<MPCFillCard> m_Frontsides;
        std::optional<QString> m_BacksideId;
    };
    struct CardParseResult
    {
        MPCFillCard m_Card;
        QList<uint32_t> m_Slots;
    };
    struct PendingRequest
    {
        QString m_Name;
        QString m_Uri;
    };

    static QString MPCFillIdFromUrl(const QString& url);
    static CardParseResult ParseMPCFillCard(const QDomElement& element);

    bool PushSingleRequest();

    MPCFillSet m_Set{};
    std::unordered_map<QString, std::vector<QString>> m_Duplicates;

    QNetworkAccessManager* m_NetworkManager{ nullptr };

    size_t m_TotalRequests{};
    size_t m_FinishedRequests{};
    std::vector<PendingRequest> m_PendingRequests{};
};
