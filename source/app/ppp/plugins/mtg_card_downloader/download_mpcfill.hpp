#pragma once

#include <optional>

#include <QString>

#include <ppp/util.hpp>

class QNetworkAccessManager;

class Image;

struct MPCFillCard
{
    QString m_Name;
    QString m_Id;
    uint32_t m_Amount;
};

struct MPCFillSet
{
    std::vector<MPCFillCard> m_Frontsides;
    QString m_BacksideId;
};

std::optional<MPCFillSet> ParseMPCFill(const QString& xml);

bool BeginDownloadMPCFill(QNetworkAccessManager& network_manager,
                          const MPCFillSet& set);
QString MPCFillIdFromUrl(const QString& url);

QByteArray ImageDataFromReply(const QByteArray& reply);
Image ImageFromReply(const QByteArray& reply);
