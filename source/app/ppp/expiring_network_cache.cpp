#include <ppp/expiring_network_cache.hpp>

#include <QDateTime>

#include <dla/literals.h>

using namespace dla::int_literals;

FixedExpiryDiskCache::FixedExpiryDiskCache(QObject* parent)
    : QNetworkDiskCache(parent)
    , m_LifeSpan(60_s * 60 * 24)
{
}

void FixedExpiryDiskCache::SetCacheLifespan(dla::time_unit lifespan)
{
    m_LifeSpan = lifespan;
}

void FixedExpiryDiskCache::updateMetaData(const QNetworkCacheMetaData& meta_data)
{
    QNetworkCacheMetaData modified_meta_data{ meta_data };

    QDateTime expiration_time{ QDateTime::currentDateTime().addSecs(qRound(m_LifeSpan / 1_s)) };
    modified_meta_data.setExpirationDate(expiration_time);

    QNetworkDiskCache::updateMetaData(modified_meta_data);
}
QIODevice* FixedExpiryDiskCache::prepare(const QNetworkCacheMetaData& meta_data)
{
    const auto now{ QDateTime::currentDateTime() };
    const auto expires{ meta_data.expirationDate() };

    if (expires.isValid() && now > expires)
    {
        return nullptr;
    }

    return QNetworkDiskCache::prepare(meta_data);
}
