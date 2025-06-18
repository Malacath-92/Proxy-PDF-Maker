#pragma once

#include <optional>

#include <QString>

#include <ppp/util.hpp>

class QNetworkAccessManager;

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
    QString m_BacksideId;
};

std::optional<MPCFillSet> ParseMPCFill(const QString& xml);

uint32_t BeginDownloadMPCFill(QNetworkAccessManager& network_manager,
                          const MPCFillSet& set);
QString MPCFillIdFromUrl(const QString& url);

QByteArray ImageDataFromReply(const QByteArray& reply);
