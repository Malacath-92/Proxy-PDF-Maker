#include <ppp/plugins/mtg_card_downloader/download_scryfall.hpp>

#include <ranges>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <ppp/version.hpp>

#include <ppp/util/log.hpp>

#include <ppp/plugins/mtg_card_downloader/decklist_parser.hpp>
#include <ppp/plugins/mtg_card_downloader/scryfall_endpoints.hpp>

ScryfallDownloader::ScryfallDownloader(std::vector<QString> skip_files,
                                       const std::optional<QString>& backside_pattern)
    : CardArtDownloader{ std::move(skip_files), backside_pattern }
{
    using namespace std::chrono_literals;
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
    m_CollectionEndpoint = std::make_unique<ScryfallCollectionEndpoint>(network_manager);
    QObject::connect(m_CollectionEndpoint.get(),
                     &ScryfallEndpoint::OnError,
                     this,
                     &ScryfallDownloader::OnError);

    m_DataEndpoint = std::make_unique<ScryfallDataEndpoint>(network_manager);
    QObject::connect(m_DataEndpoint.get(),
                     &ScryfallEndpoint::OnError,
                     this,
                     &ScryfallDownloader::OnError);

    if (!m_Queries.empty())
    {
        m_SearchEndpoint = std::make_unique<ScryfallSearchEndpoint>(network_manager);
        QObject::connect(m_SearchEndpoint.get(),
                         &ScryfallEndpoint::OnError,
                         this,
                         &ScryfallDownloader::OnError);

        m_TotalRequests = m_Queries.size();
        RunQueries();
    }
    else
    {
        m_TotalRequests = static_cast<uint32_t>(m_Cards.size()) * 2;
        DownloadCardDatas();
    }

    Progress(0, static_cast<int>(m_TotalRequests));

    return true;
}

void ScryfallDownloader::HandleReply(QNetworkReply* /*reply*/)
{
}

std::vector<QString> ScryfallDownloader::GetFiles() const
{
    auto cards{ m_Cards |
                std::views::transform(&DecklistCard::m_FileName) |
                std::ranges::to<std::vector>() };
    cards.insert(cards.end(), m_BacksideFiles.begin(), m_BacksideFiles.end());
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
            return CardBackFilename(card_info["card_back_id"].toString());
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

void ScryfallDownloader::RunQueries()
{
    const size_t pending_queries{ m_Queries.size() };
    for (const QString& query : m_Queries)
    {
        m_SearchEndpoint->Queue(
            query,
            [this, query, pending_queries](const QJsonDocument& doc)
            {
                // Using curly-braces for initializers here makes gcc and clang
                // create an array-of-array, hence we are forced to use parens
                const auto data(doc["data"].toArray());
                LogInfo("Adding {} cards for query {}...",
                        data.size(),
                        query.toStdString());

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
                                             .arg(card_collector_number.rightJustified(4, '0'))
                                             .replace(QRegularExpression{ "[/\\:*?\"<>|]" }, "_") };
                    m_Cards.push_back(DecklistCard{
                        .m_Name{ std::move(card_name) },
                        .m_FileName{ std::move(card_file_name) },
                        .m_Amount = 1,
                        .m_Set{ std::move(card_set) },
                        .m_CollectorNumber{ std::move(card_collector_number) },
                    });
                    m_TotalRequests += 2;
                }

                ++m_Progress;

                Progress(static_cast<int>(m_Progress),
                         static_cast<int>(m_TotalRequests));

                if (m_Progress == pending_queries)
                {
                    for (size_t i = 0; i < m_Cards.size(); i++)
                    {
                        m_FileNameIndexMap[m_Cards[i].m_FileName] = i;
                    }
                    DownloadCardDatas();
                }
            });
    }
}
void ScryfallDownloader::DownloadCardDatas()
{
    m_CardInfos.resize(m_Cards.size());

    static constexpr auto c_BatchSize{ ScryfallCollectionEndpoint::c_BatchSize };

    const size_t target_progress{ m_Progress + m_Cards.size() };
    for (size_t batch_begin{ 0 }; batch_begin < m_Cards.size(); batch_begin += c_BatchSize)
    {
        QJsonArray identifiers{};
        for (size_t card_idx{ 0 };
             card_idx < c_BatchSize && batch_begin + card_idx < m_Cards.size();
             ++card_idx)
        {
            const auto& card{ m_Cards[batch_begin + card_idx] };

            QJsonObject identifier{};
            identifier["name"] = card.m_Name;
            if (card.m_Set.has_value())
            {
                identifier["set"] = card.m_Set.value();
            }
            else if (card.m_CollectorNumber.has_value())
            {
                identifier["collector_number"] = card.m_CollectorNumber.value();
            }
            identifiers.push_back(std::move(identifier));
        }

        QJsonObject batch_json;
        batch_json["identifiers"] = std::move(identifiers);

        m_CollectionEndpoint->Queue(
            QJsonDocument{ std::move(batch_json) },
            [this, batch_begin, target_progress](const QJsonDocument& batch, const QJsonDocument& result)
            {
                // Using curly-braces for initializers here makes gcc and clang
                // create an array-of-array, hence we are forced to use parens
                const auto identifiers(batch["identifiers"].toArray());
                const auto not_found(result["not_found"].toArray());
                const auto data(result["data"].toArray());

                size_t data_idx{ 0 };
                for (qsizetype card_idx{ 0 }; card_idx < identifiers.size(); ++card_idx, ++m_Progress)
                {
                    if (!not_found.isEmpty())
                    {
                        if (not_found.contains(identifiers[card_idx]))
                        {
                            continue;
                        }
                    }

                    m_CardInfos[batch_begin + card_idx] = QJsonDocument{ std::move(data[data_idx].toObject()) };
                    ++data_idx;
                }

                Progress(static_cast<int>(m_Progress),
                         static_cast<int>(m_TotalRequests));

                if (m_Progress == target_progress)
                {
                    DownloadCardImages();
                }
            });
    }
}
void ScryfallDownloader::DownloadCardImages()
{
    for (const auto& [card, card_info] : std::views::zip(m_Cards, m_CardInfos))
    {
        const bool has_backside{ HasBackside(card_info) };

        if (card_info.isNull())
        {
            LogError("Card {} not resolved, no image(s) downloaded...", card.m_Name.toStdString());
            ++m_Progress;
            Progress(static_cast<int>(m_Progress),
                     static_cast<int>(m_TotalRequests));
            continue;
        }

        if (std::ranges::contains(m_SkipFiles, card.m_FileName))
        {
            LogInfo("Skipping card {}", card.m_Name.toStdString());
            ++m_Progress;
            Progress(static_cast<int>(m_Progress),
                     static_cast<int>(m_TotalRequests));
            continue;
        }

        auto image_available{
            [this](const QString& file_name, const QByteArray& data)
            {
                LogInfo("Received data of {}", file_name.toStdString());
                ImageAvailable(data, file_name);
                ++m_Progress;
                Progress(static_cast<int>(m_Progress),
                         static_cast<int>(m_TotalRequests));
            },
        };

        LogInfo("Requesting artwork for card {}", card.m_Name.toStdString());
        if (has_backside)
        {
            const auto& card_faces{ card_info["card_faces"] };
            const auto& request_uri{ card_faces[0]["image_uris"]["png"] };
            m_DataEndpoint->Call(request_uri.toString(),
                                 [&card, image_available](const QByteArray& data)
                                 {
                                     image_available(card.m_FileName, data);
                                 });

            if (IncludeBacksides())
            {
                if (has_backside &&
                    !std::ranges::contains(m_SkipFiles, BacksideFilename(card.m_FileName)))
                {
                    LogInfo("Requesting backside artwork for card {}", card.m_Name.toStdString());

                    const auto& back_face_uri{ card_faces[1]["image_uris"]["png"] };
                    const auto backside_file_name{ BacksideFilename(card.m_FileName) };
                    m_BacksideFiles.push_back(backside_file_name);

                    ++m_TotalRequests;
                    m_DataEndpoint->Call(back_face_uri.toString(),
                                         [backside_file_name, image_available](const QByteArray& data)
                                         {
                                             image_available(backside_file_name, data);
                                         });
                }
            }
        }
        else
        {
            const auto& request_uri{ card_info["image_uris"]["png"] };
            m_DataEndpoint->Call(request_uri.toString(),
                                 [card, image_available](const QByteArray& data)
                                 {
                                     image_available(card.m_FileName, data);
                                 });

            if (IncludeBacksides() && card_info["card_back_id"].isString())
            {
                const auto& card_back_id{ card_info["card_back_id"].toString() };
                if (!std::ranges::contains(m_SkipFiles, card_back_id))
                {
                    LogInfo("Requesting card back {}", card_back_id.toStdString());

                    const auto card_back_uri{ QString{ "https://backs.scryfall.io/png/%1/%2/%3.png" }
                                                  .arg(card_back_id[0])
                                                  .arg(card_back_id[1])
                                                  .arg(card_back_id) };
                    const auto backside_file_name{ CardBackFilename(card_back_id) };

                    m_BacksideFiles.push_back(backside_file_name);
                    m_SkipFiles.push_back(card_back_id);

                    ++m_TotalRequests;
                    m_DataEndpoint->Call(card_back_uri,
                                         [backside_file_name, image_available](const QByteArray& data)
                                         {
                                             image_available(backside_file_name, data);
                                         });
                }
            }
        }
    }
}
