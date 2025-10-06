#pragma once

#include <optional>
#include <vector>

#include <QList>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class CardArtDownloader : public QObject
{
    Q_OBJECT

  public:
    CardArtDownloader(std::vector<QString> skip_files, QString backside_pattern);
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

  protected:
    QString BacksideFilename(const QString& file_name) const;

    std::vector<QString> m_SkipFiles;

  private:
    QString m_BacksidePrefix;
    QString m_BacksideSuffix;
};

inline CardArtDownloader::CardArtDownloader(std::vector<QString> skip_files, QString backside_pattern)
    : m_SkipFiles{ std::move(skip_files) }
    , m_BacksidePrefix{ backside_pattern.split('$')[0] }
    , m_BacksideSuffix{ backside_pattern.split('$')[1] }
{
}

inline QString CardArtDownloader::BacksideFilename(const QString& file_name) const
{
    return m_BacksidePrefix + file_name + m_BacksideSuffix;
}
