#pragma once

#include <QString>

enum class DecklistType
{
    None,
    RawNames,
    MODO,
    NamesAndSet,
    MTGA,
    Archidekt,
    Moxfield,
};

struct DecklistCard
{
    QString m_Name;
    QString m_FileName;
    uint32_t m_Amount;

    std::optional<QString> m_Set;
    std::optional<QString> m_CollectorNumber;
};

std::vector<DecklistCard> ParseDecklist(DecklistType type, const QString& decklist);
DecklistType InferDecklistType(const QString& decklist);
