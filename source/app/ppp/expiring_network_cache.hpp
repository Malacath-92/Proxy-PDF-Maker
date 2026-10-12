#pragma once

#include <QNetworkDiskCache>

#include <dla/units.h>

class FixedExpiryDiskCache : public QNetworkDiskCache
{
    Q_OBJECT
  public:
    explicit FixedExpiryDiskCache(QObject* parent = nullptr);

    void SetCacheLifespan(dla::time_unit lifespan);

    virtual void updateMetaData(const QNetworkCacheMetaData& meta_data) override;
    virtual QIODevice* prepare(const QNetworkCacheMetaData& meta_data) override;

  private:
    dla::time_unit m_LifeSpan;
};
