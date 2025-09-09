#include <ppp/plugins/mtg_card_downloader/download_decklist.hpp>

#include <ranges>
#include <vector>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ppp/util/log.hpp>

#include <ppp/plugins/mtg_card_downloader/decklist_parser.hpp>

struct ScryfallDownloader::BacksideRequest
{
    QString m_Uri;
    DecklistCard m_Front;
};

ScryfallDownloader::ScryfallDownloader(std::vector<QString> skip_files)
    : m_SkipFiles{ std::move(skip_files) }
{
    m_ScryfallTimer.setInterval(100);
    m_ScryfallTimer.setSingleShot(true);

    QObject::connect(&m_ScryfallTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         NextRequest();
                     });
}
ScryfallDownloader::~ScryfallDownloader() = default;

bool ScryfallDownloader::ParseInput(const QString& decklist)
{
    auto decklist_type{ InferDecklistType(decklist) };
    m_Cards = ParseDecklist(decklist_type, decklist);

    for (size_t i = 0; i < m_Cards.size(); i++)
    {
        m_FileNameIndexMap[m_Cards[i].m_FileName] = i;
    }

    return true;
}

bool ScryfallDownloader::BeginDownload(QNetworkAccessManager& network_manager)
{
    m_TotalRequests = static_cast<uint32_t>(m_Cards.size()) * 2;

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
        auto reply_json{ QJsonDocument::fromJson(reply->readAll()) };
        if (!m_Cards[m_CardInfos.size()].m_CollectorNumber.has_value())
        {
            m_CardInfos.push_back(QJsonDocument{
                reply_json["data"][0].toObject(),
            });
        }
        else
        {
            m_CardInfos.push_back(std::move(reply_json));
        }

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
    auto cards{ m_Cards |
                std::views::transform(&DecklistCard::m_FileName) |
                std::ranges::to<std::vector>() };
    auto backsides{
        m_Backsides |
        std::views::transform(&BacksideRequest::m_Front) |
        std::views::transform(&DecklistCard::m_FileName) |
        std::views::transform(&ScryfallDownloader::BacksideFilename)
    };
    cards.insert(cards.end(), backsides.begin(), backsides.end());
    return cards;
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

    const auto it{ std::ranges::find(m_Cards, file_name, &DecklistCard::m_FileName) };
    if (it != m_Cards.end())
    {
        const auto idx{ it - m_Cards.begin() };
        const auto card_info{ m_CardInfos[idx] };
        if (card_info["card_back_id"].isString())
        {
            return BacksideFilename(CardBackFilename(card_info["card_back_id"].toString()));
        }
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
    return "__back_" + file_name;
}

QString ScryfallDownloader::CardBackFilename(const QString& card_back_id)
{
    return card_back_id + ".png";
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
        LogInfo("Requesting info for card {}", card.m_Name.toStdString());

        if (!card.m_Set.has_value())
        {
            auto request_uri{
                QString("https://api.scryfall.com/cards/named?exact=%1")
                    .arg(card.m_Name),
            };
            do_request(std::move(request_uri));
        }
        else if (!card.m_CollectorNumber.has_value())
        {
            auto request_uri{
                QString(R"(https://api.scryfall.com/cards/search?q=!"%1"+set:%2&unique=prints)")
                    .arg(card.m_Name)
                    .arg(card.m_Set.value()),
            };
            do_request(std::move(request_uri));
        }
        else
        {
            auto request_uri{
                QString("https://api.scryfall.com/cards/%1/%2")
                    .arg(card.m_Set.value())
                    .arg(card.m_CollectorNumber.value()),
            };
            do_request(std::move(request_uri));
        }
        return true;
    }
    else if (want_more_arts)
    {
        const auto& card{ m_Cards[m_Downloads] };
        const auto& card_info{ m_CardInfos[m_Downloads] };
        const bool has_backside{ HasBackside(card_info) };

        if (has_backside &&
            !std::ranges::contains(m_SkipFiles, BacksideFilename(card.m_FileName)))
        {
            const auto& card_faces{ card_info["card_faces"] };
            const auto& back_face_uri{ card_faces[1]["image_uris"]["png"] };

            m_Backsides.push_back(BacksideRequest{
                .m_Uri{ back_face_uri.toString() },
                .m_Front{ card },
            });
            ++m_TotalRequests;
        }

        if (std::ranges::contains(m_SkipFiles, card.m_FileName))
        {
            LogInfo("Skipping card {}", card.m_Name.toStdString());
            m_Downloads++;
            Progress(static_cast<int>(m_Downloads + m_CardInfos.size()),
                     static_cast<int>(m_TotalRequests));
            return NextRequest();
        }
        else
        {
            LogInfo("Requesting artwork for card {}", card.m_Name.toStdString());
            if (HasBackside(card_info))
            {
                const auto& card_faces{ card_info["card_faces"] };
                const auto& request_uri{ card_faces[0]["image_uris"]["png"] };
                do_request(request_uri.toString());
            }
            else
            {
                const auto& request_uri{ card_info["image_uris"]["png"] };
                do_request(request_uri.toString());

                if (card_info["card_back_id"].isString())
                {
                    LogInfo("Requesting card back id for card {}", card.m_Name.toStdString());
                    const auto& card_back_id{ card_info["card_back_id"].toString() };

                    if (!std::ranges::contains(m_SkipFiles, card_back_id))
                    {
                        const auto card_back_uri{ QString{ "https://backs.scryfall.io/png/%1/%2/%3.png" }
                                                      .arg(card_back_id[0])
                                                      .arg(card_back_id[1])
                                                      .arg(card_back_id) };

                        m_Backsides.push_back(BacksideRequest{
                            .m_Uri{ card_back_uri },
                            .m_Front{
                                .m_Name{ card_back_id },
                                .m_FileName{ CardBackFilename(card_back_id) },
                                .m_Amount = 0,

                                .m_Set{ std::nullopt },
                                .m_CollectorNumber{ std::nullopt },
                            },
                        });
                        ++m_TotalRequests;

                        m_SkipFiles.push_back(card_back_id);
                    }
                }
            }
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
