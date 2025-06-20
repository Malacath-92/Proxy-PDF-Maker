#pragma once

#include <optional>
#include <vector>

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class CardArtDownloader : public QObject
{
    Q_OBJECT

  public:
    virtual ~CardArtDownloader() = default;

    virtual bool ParseInput(const QString& xml) = 0;
    virtual bool BeginDownload(QNetworkAccessManager& network_manager) = 0;
    virtual void HandleReply(QNetworkReply* reply) = 0;

    virtual std::vector<QString> GetFiles() const = 0;

    virtual uint32_t GetAmount(const QString& file_name) const = 0;
    virtual std::optional<QString> GetBackside(const QString& file_name) const = 0;
    virtual std::vector<QString> GetDuplicates(const QString& file_name) const = 0;

    virtual bool ProvidesBleedEdge() const = 0;

  signals:
    void Progress(int progress, int target);
    void ImageAvailable(const QByteArray& image_data, const QString& file_name);
};
