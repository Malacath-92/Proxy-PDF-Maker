#include <ppp/plugins/mtg_card_downloader/download_scryfall.hpp>

#include <ranges>
#include <vector>

#include <QFile>
#include <QJsonArray>
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

ScryfallDownloader::ScryfallDownloader(std::vector<QString> skip_files,
                                       const std::optional<QString>& backside_pattern)
    : CardArtDownloader{ std::move(skip_files), backside_pattern }
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

bool ScryfallDownloader::ParseInput(const QString& input)
{
    if (input.startsWith("$"))
    {
        const auto lines{
            input.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
        };
        for (const auto& line : lines)
        {
            if (line.startsWith("$"))
            {
                m_Queries.push_back(line.sliced(1));
            }
        }
    }
    else
    {
        auto decklist_type{ InferDecklistType(input) };
        m_Cards = ParseDecklist(decklist_type, input);

        for (size_t i = 0; i < m_Cards.size(); i++)
        {
            m_FileNameIndexMap[m_Cards[i].m_FileName] = i;
        }
    }

    return true;
}

bool ScryfallDownloader::BeginDownload(QNetworkAccessManager& network_manager)
{
    if (!m_Queries.empty())
    {
        m_TotalRequests = m_Queries.size();
    }
    else
    {
        m_TotalRequests = static_cast<uint32_t>(m_Cards.size()) * 2;
    }

    Progress(0, static_cast<int>(m_TotalRequests));

    m_NetworkManager = &network_manager;

    return NextRequest();
}

void ScryfallDownloader::HandleReply(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NetworkError::NoError)
    {
        return;
    }

    if (!m_Queries.empty())
    {
        auto reply_json{ QJsonDocument::fromJson(reply->readAll()) };

        const auto data{ reply_json["data"].toArray() };
        LogInfo("Adding {} cards for query {}...",
                data.size(),
                m_Queries.back().toStdString());

        for (const auto& card : data)
        {
            const auto card_obj{ card.toObject() };
            auto card_name{
                [&]()
                {
                    if (card_obj.contains("card_faces"))
                    {
                        return card_obj["card_faces"][0]["name"].toString();
                    }
                    return card_obj["name"].toString();
                }()
            };
            auto card_set{ card_obj["set"].toString() };
            auto card_collector_number{ card_obj["collector_number"].toString() };
            auto card_file_name{ QString{ "%2 (%3) - %1.png" }
                                     .arg(card_name)
                                     .arg(card_set.toUpper())
                                     .arg(card_collector_number.rightJustified(4, '0')) };
            m_Cards.push_back(DecklistCard{
                .m_Name{ std::move(card_name) },
                .m_FileName{ std::move(card_file_name) },
                .m_Amount = 1,
                .m_Set{ std::move(card_set) },
                .m_CollectorNumber{ std::move(card_collector_number) },
            });
            m_TotalRequests += 2;
        }

        if (reply_json["has_more"].toBool())
        {
            m_QueryMoreData = reply_json["next_page"].toString();
            LogInfo("Query {} has more data, fetching next page...",
                    m_Queries.back().toStdString());
        }
        else
        {
            m_Queries.pop_back();
            m_Progress++;
        }
    }
    else
    {
        const auto& meta_data{ m_ReplyMetaData[reply] };
        if (meta_data.m_Type == RequestType::CardInfo)
        {
            const auto& card{ m_Cards[meta_data.m_Index] };
            auto reply_json{ QJsonDocument::fromJson(reply->readAll()) };
            if (!card.m_Set.has_value())
            {
                m_CardInfos.push_back(std::move(reply_json));
            }
            else if (!card.m_CollectorNumber.has_value())
            {
                m_CardInfos.push_back(QJsonDocument{
                    reply_json["data"][0].toObject(),
                });
            }
            else
            {
                m_CardInfos.push_back(std::move(reply_json));
            }

            m_Progress++;
        }
        else
        {
            auto image_available{
                [this, reply](const QString& file_name)
                {
                    LogInfo("Received data of {}", file_name.toStdString());
                    ImageAvailable(reply->readAll(), file_name);
                    m_Downloads++;
                    m_Progress++;
                },
            };
            if (meta_data.m_Type == RequestType::CardImage)
            {
                const auto& card{ m_Cards[meta_data.m_Index] };
                image_available(card.m_FileName);
            }
            else if (meta_data.m_Type == RequestType::BacksideImage)
            {
                const auto& backside_card{ m_Backsides[meta_data.m_Index] };
                const auto file_name{ BacksideFilename(backside_card.m_Front.m_FileName) };
                image_available(file_name);
            }
        }
    }

    Progress(static_cast<int>(m_Progress),
             static_cast<int>(m_TotalRequests));

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
        std::views::transform([this](const QString& n)
                              { return BacksideFilename(n); })
    };
    cards.insert(cards.end(), backsides.begin(), backsides.end());
    return cards;
}

uint32_t ScryfallDownloader::GetAmount(const QString& file_name) const
{
    if (!m_FileNameIndexMap.contains(file_name))
    {
        return 0;
    }

    return m_Cards[m_FileNameIndexMap.at(file_name)].m_Amount;
}

std::optional<QString> ScryfallDownloader::GetBackside(const QString& file_name) const
{
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

QString ScryfallDownloader::CardBackFilename(const QString& card_back_id)
{
    return card_back_id + ".png";
}

QNetworkReply* ScryfallDownloader::DoRequestWithMetadata(QString request_uri,
                                                         QString file_name,
                                                         size_t index,
                                                         RequestType request_type)
{
    auto* reply{ DoRequestWithoutMetadata(std::move(request_uri)) };
    m_ReplyMetaData[reply] = MetaData{
        std::move(file_name),
        index,
        request_type,
    };
    return reply;
}

QNetworkReply* ScryfallDownloader::DoRequestWithoutMetadata(QString request_uri)
{
    if (request_uri.isEmpty())
    {
        LogError("Empty request... Download cancelled...");
        return nullptr;
    }

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

    return reply;
}

bool ScryfallDownloader::NextRequest()
{
    if (m_QueryMoreData.has_value())
    {
        DoRequestWithoutMetadata(std::move(m_QueryMoreData.value()));
        m_QueryMoreData.reset();
        return true;
    }

    if (!m_Queries.empty())
    {
        DoRequestWithoutMetadata(QString("https://api.scryfall.com/cards/search?q=%1")
                                     .arg(QUrl::toPercentEncoding(m_Queries.back())));
        return true;
    }

    const auto cards_in_deck{ m_Cards.size() };
    const bool want_more_infos{ m_CardInfos.size() < cards_in_deck };
    const bool want_more_arts{ m_Downloads < cards_in_deck };
    const bool want_more_back_arts{ m_Downloads < cards_in_deck + m_Backsides.size() };
    if (want_more_infos)
    {
        const size_t index{ m_CardInfos.size() };
        const auto& card{ m_Cards[index] };
        LogInfo("Requesting info for card {}", card.m_Name.toStdString());

        auto request_uri{
            [&]()
            {
                if (!card.m_Set.has_value())
                {
                    return QString("https://api.scryfall.com/cards/named?exact=%1")
                        .arg(card.m_Name);
                }
                else if (!card.m_CollectorNumber.has_value())
                {
                    return QString(R"(https://api.scryfall.com/cards/search?q=!"%1"+set:%2&unique=prints)")
                        .arg(card.m_Name)
                        .arg(card.m_Set.value());
                }
                else
                {
                    return QString("https://api.scryfall.com/cards/%1/%2")
                        .arg(card.m_Set.value())
                        .arg(card.m_CollectorNumber.value());
                }
            }()
        };
        DoRequestWithMetadata(std::move(request_uri), card.m_FileName, index, RequestType::CardInfo);
        return true;
    }
    else if (want_more_arts)
    {
        const size_t index{ m_Downloads };
        const auto& card{ m_Cards[index] };
        const auto& card_info{ m_CardInfos[index] };
        const bool has_backside{ HasBackside(card_info) };

        if (std::ranges::contains(m_SkipFiles, card.m_FileName))
        {
            LogInfo("Skipping card {}", card.m_Name.toStdString());
            m_Downloads++;
            m_Progress++;
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
                DoRequestWithMetadata(request_uri.toString(), card.m_FileName, index, RequestType::CardImage);

                if (IncludeBacksides())
                {
                    if (has_backside &&
                        !std::ranges::contains(m_SkipFiles, BacksideFilename(card.m_FileName)))
                    {
                        const auto& back_face_uri{ card_faces[1]["image_uris"]["png"] };

                        m_Backsides.push_back(BacksideRequest{
                            .m_Uri{ back_face_uri.toString() },
                            .m_Front{ card },
                        });
                        ++m_TotalRequests;
                    }
                }
            }
            else
            {
                const auto& request_uri{ card_info["image_uris"]["png"] };
                DoRequestWithMetadata(request_uri.toString(), card.m_FileName, index, RequestType::CardImage);

                if (IncludeBacksides())
                {
                    if (card_info["card_back_id"].isString())
                    {
                        const auto& card_back_id{ card_info["card_back_id"].toString() };
                        LogInfo("Requesting card back id for card {}", card.m_Name.toStdString());

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
        }

        return true;
    }
    else if (want_more_back_arts)
    {
        const size_t index{ m_Downloads - cards_in_deck };
        const auto& backside_card{ m_Backsides[index] };
        LogInfo("Requesting backside artwork for card {}", backside_card.m_Front.m_Name.toStdString());
        DoRequestWithMetadata(backside_card.m_Uri, BacksideFilename(backside_card.m_Front.m_FileName), index, RequestType::BacksideImage);

        return true;
    }
    return false;
}
