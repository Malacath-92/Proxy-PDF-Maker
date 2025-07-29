#include <ppp/plugins/mtg_card_downloader/download_decklist.hpp>

#include <ranges>

#include <QFile>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ppp/util/log.hpp>

struct ScryfallDownloader::DecklistCard
{
    QString m_Name;
    QString m_FileName;
    uint32_t m_Amount;

    std::optional<QString> m_Set;
    std::optional<QString> m_CollectorNumber;

    bool m_HasBackside{ false };
};

struct ScryfallDownloader::BacksideRequest
{
    QString m_Uri;
    DecklistCard m_Front;
};

ScryfallDownloader::ScryfallDownloader()
{
    m_ScryfallTimer.setInterval(100);
    m_ScryfallTimer.setSingleShot(true);

    QObject::connect(&m_ScryfallTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         NextRequest();
                         // TODO: More asynch
                         // if (NextRequest())
                         //{
                         //    m_ScryfallTimer.start();
                         //}
                     });
}
ScryfallDownloader::~ScryfallDownloader() = default;

bool ScryfallDownloader::ParseInput(const QString& decklist)
{
    const auto decklist_regex{ GetDecklistRegex() };
    const auto lines{
        decklist.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
    };

    for (const auto& line : lines)
    {
        const auto match{ decklist_regex.match(line) };
        if (match.hasMatch())
        {
            m_Cards.push_back(ParseDeckline(match));
            m_FileNameIndexMap[m_Cards.back().m_FileName] = m_Cards.size() - 1;
        }
    }

    return true;
}

bool ScryfallDownloader::BeginDownload(QNetworkAccessManager& network_manager)
{
    m_Backsides.push_back(BacksideRequest{
        .m_Uri{ "http://cards.scryfall.io/back.png" },
        .m_Front{},
    });

    m_TotalRequests = static_cast<uint32_t>(m_Cards.size()) * 2 + 1;
    Progress(0, static_cast<int>(m_TotalRequests));

    m_NetworkManager = &network_manager;

    return NextRequest();
}

void ScryfallDownloader::HandleReply(QNetworkReply* reply)
{
    const auto cards_in_deck{ m_Cards.size() };
    const bool waiting_for_infos{ m_CardInfos.size() < cards_in_deck };
    if (waiting_for_infos)
    {
        m_CardInfos.push_back(QJsonDocument::fromJson(reply->readAll()));

        Progress(static_cast<int>(m_CardInfos.size()),
                 static_cast<int>(m_TotalRequests));
    }
    else
    {
        const bool waiting_for_arts{ m_Downloads < cards_in_deck };
        const bool waiting_for_back_arts{ m_Downloads < cards_in_deck + m_Backsides.size() };

        auto image_available{
            [this, reply](const QString& file_name)
            {
                LogInfo("Received data of {}", file_name.toStdString());
                ImageAvailable(reply->readAll(), file_name);
                ++m_Downloads;
                Progress(static_cast<int>(m_Downloads + m_CardInfos.size()),
                         static_cast<int>(m_TotalRequests));
            },
        };
        if (waiting_for_arts)
        {
            const auto& card{ m_Cards[m_Downloads] };
            image_available(card.m_FileName);
        }
        else if (waiting_for_back_arts)
        {
            const auto& backside_card{ m_Backsides[m_Downloads - cards_in_deck] };
            const auto file_name{ BacksideFilename(backside_card.m_Front.m_FileName) };
            image_available(file_name);
        }
    }

    reply->deleteLater();
    m_ScryfallTimer.start();
}

std::vector<QString> ScryfallDownloader::GetFiles() const
{
    return m_Cards |
           std::views::transform(&DecklistCard::m_FileName) |
           std::ranges::to<std::vector>();
}

uint32_t ScryfallDownloader::GetAmount(const QString& file_name) const
{
    return m_Cards[m_FileNameIndexMap.at(file_name)].m_Amount;
}

std::optional<QString> ScryfallDownloader::GetBackside(const QString& file_name) const
{
    if (HasBackside(m_CardInfos[m_FileNameIndexMap.at(file_name)]))
    {
        return BacksideFilename(file_name);
    }
    return std::nullopt;
}

std::vector<QString> ScryfallDownloader::GetDuplicates(const QString& /*file_name*/) const
{
    return {};
}

bool ScryfallDownloader::ProvidesBleedEdge() const
{
    return false;
}

QRegularExpression ScryfallDownloader::GetDecklistRegex()
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

ScryfallDownloader::DecklistCard ScryfallDownloader::ParseDeckline(const QRegularExpressionMatch& deckline)
{
    DecklistCard card{
        .m_Name{ deckline.captured(2) },
        .m_FileName{},
        .m_Amount = deckline.capturedView(1).toUInt(),

        .m_Set{ deckline.captured(3) },
        .m_CollectorNumber = deckline.captured(4),
    };

    if (card.m_Set.has_value())
    {
        if (card.m_CollectorNumber.has_value())
        {
            card.m_FileName = QString{ "%1 (%2) %3.png" }
                                  .arg(card.m_Name)
                                  .arg(card.m_Set.value())
                                  .arg(card.m_CollectorNumber.value())
                                  .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
        }
        else
        {
            card.m_FileName = QString{ "%1 (%2).png" }
                                  .arg(card.m_Name)
                                  .arg(card.m_Set.value())
                                  .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
        }
    }
    else
    {
        card.m_FileName = QString{ "%1.png" }
                              .arg(card.m_Name)
                              .replace(QRegularExpression{ R"([\\/:*?\"<>|])" }, "");
    }
    return card;
}

bool ScryfallDownloader::HasBackside(const QJsonDocument& card_info)
{
    static constexpr std::array c_DoubleSidedLayouts{
        "transform",
        "modal_dfc",
        "double_faced_token",
        "reversible_card",
        // TODO: "meld",
    };

    const auto layout{ card_info["layout"].toString() };
    return std::ranges::contains(c_DoubleSidedLayouts, layout);
}

QString ScryfallDownloader::BacksideFilename(const QString& file_name)
{
    return file_name.isEmpty() ? "__back.png" : "__back_" + file_name;
}

bool ScryfallDownloader::NextRequest()
{
    auto do_request{
        [this](QString request_uri)
        {
            QNetworkRequest get_request{ std::move(request_uri) };
            QNetworkReply* reply{ m_NetworkManager->get(std::move(get_request)) };

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

    const auto cards_in_deck{ m_Cards.size() };
    const bool want_more_infos{ m_CardInfos.size() < cards_in_deck };
    const bool want_more_arts{ m_Downloads < cards_in_deck };
    const bool want_more_back_arts{ m_Downloads < cards_in_deck + m_Backsides.size() };
    if (want_more_infos)
    {
        const auto& card{ m_Cards[m_CardInfos.size()] };
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
        const auto& card{ m_Cards[m_Downloads] };
        const auto& card_info{ m_CardInfos[m_Downloads] };

        LogInfo("Requesting artwork for card {}", card.m_Name.toStdString());
        if (HasBackside(card_info))
        {
            const auto card_faces{ card_info["card_faces"] };

            auto request_uri{ card_faces[0]["image_uris"]["png"].toString() };
            do_request(std::move(request_uri));

            const auto back_face_uri{ card_faces[1]["image_uris"]["png"] };

            m_Backsides.push_back(BacksideRequest{
                .m_Uri{ back_face_uri.toString() },
                .m_Front{ card },
            });
            ++m_TotalRequests;
        }
        else
        {
            auto request_uri{ card_info["image_uris"]["png"].toString() };
            do_request(std::move(request_uri));
        }

        return true;
    }
    else if (want_more_back_arts)
    {
        const auto& backside_card{ m_Backsides[m_Downloads - cards_in_deck] };
        LogInfo("Requesting backside artwork for card {}", backside_card.m_Front.m_Name.toStdString());
        do_request(backside_card.m_Uri);

        return true;
    }
    return false;
}
