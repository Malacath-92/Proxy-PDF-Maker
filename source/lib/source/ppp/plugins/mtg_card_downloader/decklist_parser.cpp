#include <ppp/plugins/mtg_card_downloader/decklist_parser.hpp>

#include <optional>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QRegularExpression>

class DecklistParser
{
  public:
    virtual ~DecklistParser() = default;

    virtual bool Verify(const QString& decklist) const;
    virtual std::vector<DecklistCard> Parse(const QString& decklist) const;

  protected:
    static QString MakeFileName(const DecklistCard& card);

  private:
    virtual bool IsValidLine(const QString& line) const = 0;
    virtual uint32_t SkipLinesAfter(const QString& line) const;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const = 0;
};

/*
    Sol Ring
    Arcane Signet
    Yagwmoth's Will
*/
class RawNamesParser final : public DecklistParser
{
    virtual bool IsValidLine(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;
};

/*
    1 Sol Ring
    1 Arcane Signet
    2 Yagwmoth's Will
*/
class MODOParser final : public DecklistParser
{
    virtual bool IsValidLine(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;

    static inline QRegularExpression g_Regex{ "(\\d+)x? ([^\\(\\[]+)" };
};

/*
    1 Sol Ring (LEB)
    1 Arcane Signet (ELD)
    2 Yagwmoth's Will (USG)
*/
class NamesAndSetParser final : public DecklistParser
{
    virtual bool IsValidLine(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;

    static inline QRegularExpression g_Regex{
        "(\\d+)x? ([^\\(]+) \\(([^ ]+)\\)"
    };
};

/*
    1 Sol Ring (LEB) 270
    1 Arcane Signet (ELD) 331
    2 Yagwmoth's Will (USG) 171
*/
class MTGAParser final : public DecklistParser
{
    virtual bool IsValidLine(const QString& line) const override;
    virtual uint32_t SkipLinesAfter(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;

    static inline QRegularExpression g_Regex{
        "(\\d+)x? ([^\\(]+) \\(([^ ]+)\\) (\\S*\\d+\\S*)"
    };
};

/*
    1x Sol Ring (LEB) 270 ...
    1x Arcane Signet (ELD) 331 ...
    2x Yagwmoth's Will (USG) 171 ...
*/
class ArchidektParser final : public DecklistParser
{
    virtual bool IsValidLine(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;

    static inline QRegularExpression g_Regex{
        "(\\d+)x? ([^\\(]+) \\(([^ ]+)\\) (\\S*\\d+\\S*)"
    };
};

/*
    1 Sol Ring (LEB) 270 ...
    1 Arcane Signet (ELD) 331 ...
    2 Yagwmoth's Will (USG) 171 ...
*/
class MoxfieldParser final : public DecklistParser
{
    virtual bool IsValidLine(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;

    static inline QRegularExpression g_Regex{
        "(\\d+) ([^\\(]+) \\(([^ ]+)\\) (\\S*\\d+\\S*)"
    };
};

/*
  "object": "deck",
  "id": "fbef0384-d613-47c2-ba2b-3aab2c2d353e",
  ...
  "entries": {
    ...
  }
}
*/
class ScryfallParser final : public DecklistParser
{
  public:
    virtual bool Verify(const QString& decklist) const override;
    virtual std::vector<DecklistCard> Parse(const QString& decklist) const override;

  private:
    virtual bool IsValidLine(const QString& line) const override;
    virtual std::optional<DecklistCard> LineToCard(const QString& line) const override;
};

template<class FunT>
auto ForEachLine(const QString& text, auto default_return, FunT&& fun)
{
    const auto lines{
        text.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
    };
    for (const auto& line : lines)
    {
        auto ret{ fun(line) };
        if (ret.has_value())
        {
            return ret.value();
        }
    }

    return default_return;
}

template<class FunT>
void ForEachLine(const QString& text, FunT&& fun)
{
    ForEachLine(
        text,
        std::monostate{},
        [&](const QString& line)
        {
            fun(line);
            return std::optional<std::monostate>{};
        });
}

bool DecklistParser::Verify(const QString& decklist) const
{
    return ForEachLine(
        decklist,
        true,
        [this, skip_lines = 0u](const QString& line) mutable -> std::optional<bool>
        {
            if (skip_lines > 0)
            {
                --skip_lines;
                return std::nullopt;
            }

            if (!IsValidLine(line))
            {
                return false;
            }

            skip_lines = SkipLinesAfter(line);

            return std::nullopt;
        });
}

std::vector<DecklistCard> DecklistParser::Parse(const QString& decklist) const
{
    std::vector<DecklistCard> cards_list;
    ForEachLine(
        decklist,
        [&](const QString& line)
        {
            if (auto card{ LineToCard(line) })
            {
                if (card->m_FileName.isEmpty())
                {
                    card->m_FileName = MakeFileName(card.value());
                }
                cards_list.push_back(std::move(card).value());
            }
        });
    return cards_list;
}

QString DecklistParser::MakeFileName(const DecklistCard& card)
{
    if (card.m_Set.has_value())
    {
        if (card.m_CollectorNumber.has_value())
        {
            return QString{ "%1 (%2) %3.png" }
                .arg(card.m_Name)
                .arg(card.m_Set.value())
                .arg(card.m_CollectorNumber.value())
                .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
        }
        else
        {
            return QString{ "%1 (%2).png" }
                .arg(card.m_Name)
                .arg(card.m_Set.value())
                .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
        }
    }
    else
    {
        return QString{ "%1.png" }
            .arg(card.m_Name)
            .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
    }
}

uint32_t DecklistParser::SkipLinesAfter(const QString&) const
{
    return 0;
}

bool RawNamesParser::IsValidLine(const QString&) const
{
    return true;
}

std::optional<DecklistCard> RawNamesParser::LineToCard(const QString& line) const
{
    DecklistCard card{
        .m_Name{ line },
        .m_FileName{},
        .m_Amount = 1,

        .m_Set{},
        .m_CollectorNumber{},
    };
    return card;
}

bool MODOParser::IsValidLine(const QString& line) const
{
    return g_Regex.match(line).hasMatch();
}

std::optional<DecklistCard> MODOParser::LineToCard(const QString& line) const
{
    const auto deckline{ g_Regex.match(line) };
    DecklistCard card{
        .m_Name{ deckline.captured(2) },
        .m_FileName{},
        .m_Amount = deckline.capturedView(1).toUInt(),

        .m_Set{},
        .m_CollectorNumber{},
    };
    return card;
}

bool NamesAndSetParser::IsValidLine(const QString& line) const
{
    return g_Regex.match(line).hasMatch();
}

std::optional<DecklistCard> NamesAndSetParser::LineToCard(const QString& line) const
{
    const auto deckline{ g_Regex.match(line) };

    DecklistCard card{
        .m_Name{ deckline.captured(2) },
        .m_FileName{},
        .m_Amount = deckline.capturedView(1).toUInt(),

        .m_Set{
            !deckline.hasCaptured(3)
                ? std::optional<QString>{}
                : deckline.captured(3),
        },
        .m_CollectorNumber{},
    };
    return card;
}

bool MTGAParser::IsValidLine(const QString& line) const
{
    if (g_Regex.match(line).hasMatch())
    {
        return true;
    }

    static constexpr std::array c_AllowedNonDeckLines{
        "About",
        "Commander",
        "Companion",
        "Deck",
        "Sideboard",
    };
    return std::ranges::contains(c_AllowedNonDeckLines, line);
}

uint32_t MTGAParser::SkipLinesAfter(const QString& line) const
{
    return line == "About" ? 1 : 0;
}

std::optional<DecklistCard> MTGAParser::LineToCard(const QString& line) const
{
    const auto deckline{ g_Regex.match(line) };
    if (deckline.hasMatch())
    {
        DecklistCard card{
            .m_Name{ deckline.captured(2) },
            .m_FileName{},
            .m_Amount = deckline.capturedView(1).toUInt(),

            .m_Set{
                !deckline.hasCaptured(3)
                    ? std::optional<QString>{}
                    : deckline.captured(3),
            },
            .m_CollectorNumber{
                !deckline.hasCaptured(4)
                    ? std::optional<QString>{}
                    : deckline.captured(4),
            },
        };
        return card;
    }

    return std::nullopt;
}

bool ArchidektParser::IsValidLine(const QString& line) const
{
    return g_Regex.match(line).hasMatch();
}

std::optional<DecklistCard> ArchidektParser::LineToCard(const QString& line) const
{
    const auto deckline{ g_Regex.match(line) };
    if (deckline.hasMatch())
    {
        DecklistCard card{
            .m_Name{ deckline.captured(2) },
            .m_FileName{},
            .m_Amount = deckline.capturedView(1).toUInt(),

            .m_Set{
                !deckline.hasCaptured(3)
                    ? std::optional<QString>{}
                    : deckline.captured(3),
            },
            .m_CollectorNumber{
                !deckline.hasCaptured(4)
                    ? std::optional<QString>{}
                    : deckline.captured(4),
            },
        };
        return card;
    }

    return std::nullopt;
}

bool MoxfieldParser::IsValidLine(const QString& line) const
{
    return g_Regex.match(line).hasMatch();
}

std::optional<DecklistCard> MoxfieldParser::LineToCard(const QString& line) const
{
    const auto deckline{ g_Regex.match(line) };
    if (deckline.hasMatch())
    {
        DecklistCard card{
            .m_Name{ deckline.captured(2) },
            .m_FileName{},
            .m_Amount = deckline.capturedView(1).toUInt(),

            .m_Set{
                !deckline.hasCaptured(3)
                    ? std::optional<QString>{}
                    : deckline.captured(3),
            },
            .m_CollectorNumber{
                !deckline.hasCaptured(4)
                    ? std::optional<QString>{}
                    : deckline.captured(4),
            },
        };
        return card;
    }

    return std::nullopt;
}

bool ScryfallParser::Verify(const QString& decklist) const
{
    const auto doc{ QJsonDocument::fromJson(decklist.toUtf8()) };
    if (doc["object"] != "deck")
    {
        return false;
    }

    if (!doc["entries"].isObject())
    {
        return false;
    }

    const auto entries{ doc["entries"].toObject() };
    for (auto& key : entries.keys())
    {
        if (!entries[key].isArray())
        {
            return false;
        }
    }

    return true;
}
std::vector<DecklistCard> ScryfallParser::Parse(const QString& decklist) const
{
    std::vector<DecklistCard> cards_list;

    const auto doc{ QJsonDocument::fromJson(decklist.toUtf8()) };

    const auto entries{ doc["entries"].toObject() };
    for (auto& key : entries.keys())
    {
        const QJsonArray list(entries[key].toArray());

        for (const auto& entry : list)
        {
            const auto entry_obj{ entry.toObject() };
            if (!entry_obj.contains("card_digest") || !entry_obj["card_digest"].isObject())
            {
                continue;
            }

            const auto card_digest{ entry_obj["card_digest"].toObject() };

            const auto name{ card_digest["name"].toString("") };
            if (name.isEmpty())
            {
                continue;
            }

            const auto amount{ entry_obj["count"].toInt(-1) };
            if (amount < 0)
            {
                continue;
            }

            DecklistCard decklist_card{
                .m_Name{ name },
                .m_FileName{},
                .m_Amount{ static_cast<uint32_t>(amount) },
                .m_Set{ std::nullopt },
                .m_CollectorNumber{ std::nullopt },
            };

            if (card_digest.contains("set"))
            {
                const auto set{ card_digest["set"].toString("") };
                if (!set.isEmpty())
                {
                    decklist_card.m_Set = set;
                }
            }

            if (card_digest.contains("collector_number"))
            {
                const auto collector_number{ card_digest["collector_number"].toString("") };
                if (!collector_number.isEmpty())
                {
                    decklist_card.m_CollectorNumber = collector_number;
                }
            }

            decklist_card.m_FileName = MakeFileName(decklist_card);

            cards_list.push_back(std::move(decklist_card));
        }
    }

    return cards_list;
}

// Stubs: Not used for Scryfall json format
bool ScryfallParser::IsValidLine(const QString& /* line */) const
{
    return false;
}
std::optional<DecklistCard> ScryfallParser::LineToCard(const QString& /* line */) const
{
    return {};
}

std::vector<DecklistCard> ParseDecklist(DecklistType type, const QString& decklist)
{
    switch (type)
    {
    case DecklistType::None:
        break;
    case DecklistType::RawNames:
        return RawNamesParser{}.Parse(decklist);
    case DecklistType::MODO:
        return MODOParser{}.Parse(decklist);
    case DecklistType::NamesAndSet:
        return NamesAndSetParser{}.Parse(decklist);
    case DecklistType::MTGA:
        return MTGAParser{}.Parse(decklist);
    case DecklistType::Archidekt:
        return ArchidektParser{}.Parse(decklist);
    case DecklistType::Moxfield:
        return MoxfieldParser{}.Parse(decklist);
    case DecklistType::Scryfall:
        return ScryfallParser{}.Parse(decklist);
    }

    return {};
}

DecklistType InferDecklistType(const QString& decklist)
{
    if (ScryfallParser{}.Verify(decklist))
    {
        return DecklistType::Scryfall;
    }
    else if (MoxfieldParser{}.Verify(decklist))
    {
        return DecklistType::Moxfield;
    }
    else if (ArchidektParser{}.Verify(decklist))
    {
        return DecklistType::Archidekt;
    }
    else if (MTGAParser{}.Verify(decklist))
    {
        return DecklistType::MTGA;
    }
    else if (NamesAndSetParser{}.Verify(decklist))
    {
        return DecklistType::NamesAndSet;
    }
    else if (MODOParser{}.Verify(decklist))
    {
        return DecklistType::MODO;
    }

    return DecklistType::RawNames;
}
