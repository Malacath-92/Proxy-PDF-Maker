#pragma once

#include <optional>

#include <QRegularExpression>
#include <QString>

#include <ppp/util.hpp>

class QNetworkAccessManager;
class QNetworkReply;

struct DecklistCard
{
    QString m_Name;
    uint32_t m_Amount;

    std::optional<QString> m_Set;
    std::optional<QString> m_CollectorNumber;
};

struct Decklist
{
    std::vector<DecklistCard> m_Cards;
};

QRegularExpression GetDecklistRegex();

QString DecklistCardFilename(const DecklistCard& card);
QString DecklistCardBacksideFilename(const DecklistCard& card);

std::optional<Decklist> ParseDecklist(const QString& decklist);

struct ScryfallState
{
    ~ScryfallState();

    uint32_t m_NumRequests;

    struct ScryfallStateImpl;
    std::unique_ptr<ScryfallStateImpl> m_Impl;
};
std::unique_ptr<ScryfallState> InitScryfall(QNetworkAccessManager& network_manager,
                                            const Decklist& decklist);

bool ScryfallNextRequest(ScryfallState& state);
bool ScryfallHandleReply(ScryfallState& state, QNetworkReply& reply, const QString& output_dir);
