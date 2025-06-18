#include <ppp/plugins/mtg_card_downloader/download_mpcfill.hpp>

#include <QDomDocument>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ppp/util/log.hpp>

struct CardParseResult
{
    MPCFillCard m_Card;
    QList<uint32_t> m_Slots;
};
CardParseResult ParseMPCFillCard(const QDomElement& element)
{
    auto name{ element.firstChildElement("name").text() };
    auto id{ element.firstChildElement("id").text() };
    auto slots_str{ element.firstChildElement("slots").text().split(",") };
    auto amount{ slots_str.size() };

    auto slots_uint{ QList<uint32_t>{} };
    for (const auto& slot_str : slots_str)
    {
        slots_uint.push_back(slot_str.toUInt());
    }

    return CardParseResult{
        .m_Card{
            .m_Name{ std::move(name) },
            .m_Id{ std::move(id) },
            .m_Amount = static_cast<uint32_t>(amount),

            .m_Backside{ std::nullopt },
        },
        .m_Slots = std::move(slots_uint),
    };
}

std::optional<MPCFillSet> ParseMPCFill(const QString& xml)
{
    QDomDocument document;
    QDomDocument::ParseResult result{ document.setContent(xml) };
    if (!result)
    {
        LogError("Parse error at line {}, column {}:\n{}",
                 result.errorLine,
                 result.errorColumn,
                 result.errorMessage.toStdString());
        return std::nullopt;
    }

    if (!document.hasChildNodes())
    {
        LogError("MPCFill xml root has no children");
        return std::nullopt;
    }

    QDomElement order{ document.firstChildElement("order") };
    if (order.isNull())
    {
        LogError("No order is defined in MPCFill xml");
        return std::nullopt;
    }

    MPCFillSet set;
    {
        std::vector<MPCFillCard> temporary_card_list;

        {
            QDomElement fronts{ order.firstChildElement("fronts") };
            if (fronts.isNull())
            {
                LogError("No fronts are defined in MPCFill xml");
                return std::nullopt;
            }

            const auto cards{ fronts.childNodes() };
            for (int i = 0; i < cards.count(); i++)
            {
                const auto& card_xml{ cards.item(i).toElement() };
                auto [card, slots_uint]{ ParseMPCFillCard(card_xml) };
                card.m_Amount = 1; // 1 per slot
                for (const auto slot : slots_uint)
                {
                    if (temporary_card_list.size() < slot + 1)
                    {
                        temporary_card_list.resize(slot + 1);
                    }
                    temporary_card_list[slot] = card;
                }
            }
        }

        {
            QDomElement backs{ order.firstChildElement("backs") };
            if (!backs.isNull())
            {
                const auto cards{ backs.childNodes() };
                for (int i = 0; i < cards.count(); i++)
                {
                    const auto& card_xml{ cards.item(i).toElement() };
                    auto [backside, slots_uint]{ ParseMPCFillCard(card_xml) };
                    for (const auto slot : slots_uint)
                    {
                        if (slot < temporary_card_list.size())
                        {
                            auto& card{ temporary_card_list[slot] };
                            assert(!card.m_Backside.has_value());
                            card.m_Backside = MPCFillBackside{
                                .m_Name{ backside.m_Name },
                                .m_Id{ backside.m_Id },
                            };
                        }
                    }
                }
            }
        }

        // Consolidate all cards into one set, removing duplicates
        for (const auto& card : temporary_card_list)
        {
            const auto card_predicate{
                [&card](const auto& existing_card)
                {
                    static constexpr auto c_BacksidesEqual{
                        [](const auto& lhs, const auto& rhs)
                        {
                            // clang-format off
                            return lhs.has_value() == rhs.has_value()
                                && (!lhs.has_value() || lhs.value().m_Id == rhs.value().m_Id);
                            // clang-format on
                        }
                    };
                    return card.m_Id == existing_card.m_Id && c_BacksidesEqual(card.m_Backside, existing_card.m_Backside);
                },
            };
            const auto found_card{ std::ranges::find_if(set.m_Frontsides, card_predicate) };
            if (found_card == set.m_Frontsides.end())
            {
                set.m_Frontsides.push_back(card);
            }
            else
            {
                ++found_card->m_Amount;
            }
        }
    }

    {
        QDomElement backside{ order.firstChildElement("cardback") };
        if (backside.isNull())
        {
            LogError("No cardback is defined in MPCFill xml");
            return std::nullopt;
        }
        set.m_BacksideId = backside.text();
    }

    return set;
}

uint32_t BeginDownloadMPCFill(QNetworkAccessManager& network_manager,
                          const MPCFillSet& set)
{
    std::vector<QString> requested_ids{};
    auto do_download{
        [&network_manager, &requested_ids](const QString& name, const QString& id)
        {
            if (std::ranges::contains(requested_ids, id))
            {
                return;
            }

            static constexpr const char c_DownloadScript[]{
                "https://script.google.com/macros/s/AKfycbw8laScKBfxda2Wb0g63gkYDBdy8NWNxINoC4xDOwnCQ3JMFdruam1MdmNmN4wI5k4/exec"
            };
            auto request_uri{ QString("%1?id=%2").arg(c_DownloadScript).arg(id) };
            LogInfo("Requesting card  {}", name.toStdString());

            QNetworkRequest get_request{ std::move(request_uri) };
            QNetworkReply* reply{ network_manager.get(std::move(get_request)) };

            QObject::connect(reply,
                             &QNetworkReply::errorOccurred,
                             reply,
                             [reply](QNetworkReply::NetworkError error)
                             {
                                 LogError("Error during request {}: {}",
                                          reply->request().url().toString().toStdString(),
                                          QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
                             });

            requested_ids.push_back(id);
        }
    };

    for (const auto& card : set.m_Frontsides)
    {
        do_download(card.m_Name, card.m_Id);
        if (card.m_Backside.has_value())
        {
            do_download(card.m_Backside.value().m_Name, card.m_Backside.value().m_Id);
        }
    }
    do_download("__back.png", set.m_BacksideId);

    return static_cast<uint32_t>(requested_ids.size());
}

QString MPCFillIdFromUrl(const QString& url)
{
    return url.split("id=").back();
}

QByteArray ImageDataFromReply(const QByteArray& reply)
{
    return QByteArray::fromBase64(reply);
}
