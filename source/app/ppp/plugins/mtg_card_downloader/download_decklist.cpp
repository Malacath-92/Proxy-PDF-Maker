#include <ppp/plugins/mtg_card_downloader/download_decklist.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ppp/util/log.hpp>

QRegularExpression GetDecklistRegex()
{
    /*
        Supports following formats with
            - N is amount,
            - NAME is Name,
            - S is Set,
            - CN is Collector Number,
            - and ... is ignored stuff

        Moxfield:
        Nx Name (S) CN

        Arkidekt:
        N Name (S) CN ...
    */
    return QRegularExpression{ R"'((\d+)x? "?(.*)"? \(([^ ]+)\) ((.*-)?\d+))'" };
}

DecklistCard ParseDeckline(const QRegularExpressionMatch& deckline)
{
    return DecklistCard{
        .m_Name{ deckline.captured(2) },
        .m_Amount = deckline.capturedView(1).toUInt(),

        .m_Set{ deckline.captured(3) },
        .m_CollectorNumber = deckline.captured(4),
    };
}

QString DecklistCardFilename(const DecklistCard& card)
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
        return QString{ "%1 (%2).png" }
            .arg(card.m_Name)
            .arg(card.m_Set.value())
            .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
    }
    return QString{ "%1.png" }
        .arg(card.m_Name)
        .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
}

QString DecklistCardBacksideFilename(const DecklistCard& card)
{
    return "__back_" + DecklistCardFilename(card);
}

std::optional<Decklist> ParseDecklist(const QString& decklist)
{

    const auto decklist_regex{ GetDecklistRegex() };
    const auto lines{
        decklist.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
    };

    Decklist parsed_list;
    for (const auto& line : lines)
    {
        const auto match{ decklist_regex.match(line) };
        if (match.hasMatch())
        {
            parsed_list.m_Cards.push_back(ParseDeckline(match));
        }
    }
    return parsed_list;
}

ScryfallState::~ScryfallState() = default;

struct BacksideRequest
{
    QString m_Uri;
    DecklistCard m_Front;
};
struct ScryfallState::ScryfallStateImpl
{
    QNetworkAccessManager& m_NetworkManager;
    const Decklist& m_Decklist;

    std::vector<QJsonDocument> m_CardInfos{};
    uint32_t m_CardArts{ 0 };

    std::vector<BacksideRequest> m_Backsides{};
};

std::unique_ptr<ScryfallState> InitScryfall(QNetworkAccessManager& network_manager,
                                            const Decklist& decklist)
{
    return std::unique_ptr<ScryfallState>{
        new ScryfallState{
            .m_NumRequests = static_cast<uint32_t>(decklist.m_Cards.size()) * 2,
            .m_Impl{
                new ScryfallState::ScryfallStateImpl{
                    .m_NetworkManager{ network_manager },
                    .m_Decklist{ decklist },
                },
            },
        }
    };
}

bool ScryfallNextRequest(ScryfallState& state)
{
    auto do_request{
        [&state](QString request_uri)
        {
            QNetworkRequest get_request{ std::move(request_uri) };
            QNetworkReply* reply{ state.m_Impl->m_NetworkManager.get(std::move(get_request)) };

            QObject::connect(reply,
                             &QNetworkReply::errorOccurred,
                             reply,
                             [reply](QNetworkReply::NetworkError error)
                             {
                                 LogError("Error during request {}: {}",
                                          reply->request().url().toString().toStdString(),
                                          QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
                             });
        },
    };

    const auto cards_in_deck{ state.m_Impl->m_Decklist.m_Cards.size() };
    const bool want_more_infos{ state.m_Impl->m_CardInfos.size() < cards_in_deck };
    const bool want_more_arts{ state.m_Impl->m_CardArts < cards_in_deck };
    const bool want_more_back_arts{ state.m_Impl->m_CardArts < cards_in_deck + state.m_Impl->m_Backsides.size() };
    if (want_more_infos)
    {
        const auto& card{ state.m_Impl->m_Decklist.m_Cards[state.m_Impl->m_CardInfos.size()] };
        if (!card.m_Set.has_value())
        {
            // TODO
            return false;
        }
        else if (!card.m_CollectorNumber.has_value())
        {
            // TODO
            return false;
        }
        else
        {
            auto request_uri{
                QString("https://api.scryfall.com/cards/%1/%2")
                    .arg(card.m_Set.value())
                    .arg(card.m_CollectorNumber.value()),
            };
            LogInfo("Requesting info for card {}", card.m_Name.toStdString());
            do_request(std::move(request_uri));
            return true;
        }
    }
    else if (want_more_arts)
    {
        const auto& card{ state.m_Impl->m_Decklist.m_Cards[state.m_Impl->m_CardArts] };
        auto request_uri{
            QString("https://api.scryfall.com/cards/%1/%2?format=image&version=png")
                .arg(card.m_Set.value())
                .arg(card.m_CollectorNumber.value()),
        };
        LogInfo("Requesting artwork for card  {}", card.m_Name.toStdString());
        do_request(std::move(request_uri));

        const auto& card_info{ state.m_Impl->m_CardInfos[state.m_Impl->m_CardArts] };
        const auto layout{ card_info["layout"].toString() };

        static constexpr std::array c_DoubleSidedLayouts{
            "transform",
            "modal_dfc",
            "double_faced_token",
            "reversible_card",
        };
        if (std::ranges::contains(c_DoubleSidedLayouts, layout))
        {
            const auto card_faces{ card_info["card_faces"] };
            const auto back_face_uri{ card_faces[1]["image_uris"]["png"] };

            state.m_Impl->m_Backsides.push_back(BacksideRequest{
                .m_Uri{ back_face_uri.toString() },
                .m_Front{ card },
            });
            ++state.m_NumRequests;
        }

        // TODO: meld layout

        return true;
    }
    else if (want_more_back_arts)
    {
        const auto& backside_card{ state.m_Impl->m_Backsides[state.m_Impl->m_CardArts - cards_in_deck] };
        LogInfo("Requesting backside artwork for card  {}", backside_card.m_Front.m_Name.toStdString());
        do_request(backside_card.m_Uri);
    }
    return false;
}

bool ScryfallHandleReply(ScryfallState& state, QNetworkReply& reply, const QString& output_dir)
{
    const auto cards_in_deck{ state.m_Impl->m_Decklist.m_Cards.size() };
    const bool waiting_for_infos{ state.m_Impl->m_CardInfos.size() < cards_in_deck };
    const bool waiting_for_arts{ state.m_Impl->m_CardArts < cards_in_deck };
    const bool waiting_for_back_arts{ state.m_Impl->m_CardArts < cards_in_deck + state.m_Impl->m_Backsides.size() };
    if (waiting_for_infos)
    {
        state.m_Impl->m_CardInfos.push_back(QJsonDocument::fromJson(reply.readAll()));
        return true;
    }
    else if (waiting_for_arts)
    {
        const auto& card{ state.m_Impl->m_Decklist.m_Cards[state.m_Impl->m_CardArts] };
        const auto file_name{ DecklistCardFilename(card) };

        LogInfo("Received data of {}", card.m_Name.toStdString());
        {
            QFile file(output_dir + "/" + file_name);
            file.open(QIODevice::WriteOnly);
            file.write(reply.readAll());
        }

        ++state.m_Impl->m_CardArts;
        return true;
    }
    else if (waiting_for_back_arts)
    {
        const auto& backside_card{ state.m_Impl->m_Backsides[state.m_Impl->m_CardArts - cards_in_deck] };
        const auto file_name{ DecklistCardBacksideFilename(backside_card.m_Front) };

        LogInfo("Received data for backside of {}", backside_card.m_Front.m_Name.toStdString());
        {
            QFile file(output_dir + "/" + file_name);
            file.open(QIODevice::WriteOnly);
            file.write(reply.readAll());
        }

        ++state.m_Impl->m_CardArts;
        return true;
    }
    return false;
}
