#include <ppp/plugins/mtg_card_downloader/download_mpcfill.hpp>

#include <ranges>

#include <QDomDocument>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>

#include <ppp/util/log.hpp>

MPCFillDownloader::MPCFillDownloader(std::vector<QString> skip_files)
    : m_SkipFiles{ std::move(skip_files) }
{
}

bool MPCFillDownloader::ParseInput(const QString& xml)
{
    QDomDocument document;
    QDomDocument::ParseResult result{ document.setContent(xml) };
    if (!result)
    {
        LogError("Parse error at line {}, column {}:\n{}",
                 result.errorLine,
                 result.errorColumn,
                 result.errorMessage.toStdString());
        return false;
    }

    if (!document.hasChildNodes())
    {
        LogError("MPCFill xml root has no children");
        return false;
    }

    QDomElement order{ document.firstChildElement("order") };
    if (order.isNull())
    {
        LogError("No order is defined in MPCFill xml");
        return false;
    }

    MPCFillSet set;
    {
        std::vector<MPCFillCard> temporary_card_list;

        {
            QDomElement fronts{ order.firstChildElement("fronts") };
            if (fronts.isNull())
            {
                LogError("No fronts are defined in MPCFill xml");
                return false;
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
            return false;
        }
        set.m_BacksideId = backside.text();
    }

    m_Set = std::move(set);

    // Handle duplicates, for cards that have the same front with different backs
    for (const auto& card : m_Set.m_Frontsides)
    {
        std::vector<QString> duplicate_file_names;

        auto duplicates{
            m_Set.m_Frontsides |
                std::views::filter([&card](const auto& other_card)
                                   { return other_card.m_Name == card.m_Name; }) |
                std::views::drop(1),
        };
#if __cpp_lib_ranges_enumerate
        for (auto [i, dupe] : duplicates | std::views::enumerate)
        {
#else
        size_t i{ 0 };
        for (auto& dupe : duplicates)
        {
#endif
            static const QRegularExpression c_ExtRegex{ "(\\.\\w+)" };
            const auto replace_to{ QString{ " - Copy %1\\1" }.arg(i) };
            dupe.m_Name = QString{ dupe.m_Name }.replace(c_ExtRegex, replace_to);
            duplicate_file_names.push_back(dupe.m_Name);

#if not __cpp_lib_ranges_enumerate
            ++i;
#endif
        }

        if (!duplicate_file_names.empty())
        {
            m_Duplicates[card.m_Name] = std::move(duplicate_file_names);
        }
    }

    return true;
}

bool MPCFillDownloader::BeginDownload(QNetworkAccessManager& network_manager)
{
    if (m_Set.m_Frontsides.empty())
    {
        return false;
    }

    m_NetworkManager = &network_manager;

    std::vector<QString> requested_ids{};
    auto queue_download{
        [this, &network_manager, &requested_ids](const QString& name, const QString& id)
        {
            if (std::ranges::contains(requested_ids, id))
            {
                return;
            }

            if (std::ranges::contains(m_SkipFiles, name))
            {
                LogInfo("Skipping download of card {}", name.toStdString());
                requested_ids.push_back(id);
                return;
            }

            static constexpr const char c_DownloadScript[]{
                "https://script.google.com/macros/s/AKfycbw8laScKBfxda2Wb0g63gkYDBdy8NWNxINoC4xDOwnCQ3JMFdruam1MdmNmN4wI5k4/exec"
            };
            auto request_uri{ QString("%1?id=%2").arg(c_DownloadScript).arg(id) };
            m_PendingRequests.push_back({
                name,
                request_uri,
            });
            requested_ids.push_back(id);
        }
    };

    for (const auto& card : m_Set.m_Frontsides)
    {
        queue_download(card.m_Name, card.m_Id);
        if (card.m_Backside.has_value())
        {
            queue_download(card.m_Backside.value().m_Name, card.m_Backside.value().m_Id);
        }
    }
    queue_download("__back.png", m_Set.m_BacksideId);

    m_TotalRequests = m_PendingRequests.size();
    Progress(0, static_cast<int>(m_TotalRequests));

    const auto c_MaxDownloadsInFlight{ 15 };
    for (size_t i = 0; i < c_MaxDownloadsInFlight; ++i)
    {
        PushSingleRequest();
    }

    return true;
}

void MPCFillDownloader::HandleReply(QNetworkReply* reply)
{
    const auto file_name{
        [this, reply]()
        {
            const QString id{ MPCFillIdFromUrl(reply->request().url().toString()) };
            for (const auto& card : m_Set.m_Frontsides)
            {
                if (card.m_Id == id)
                {
                    return card.m_Name;
                }

                if (card.m_Backside.has_value() && card.m_Backside.value().m_Id == id)
                {
                    return card.m_Backside.value().m_Name;
                }
            }

            return QString{ "__back.png" };
        }()
    };

    ImageAvailable(QByteArray::fromBase64(reply->readAll()), file_name);

    ++m_FinishedRequests;
    Progress(static_cast<int>(m_FinishedRequests),
             static_cast<int>(m_TotalRequests));

    reply->deleteLater();

    PushSingleRequest();
}

std::vector<QString> MPCFillDownloader::GetFiles() const
{
    auto frontsides{
        m_Set.m_Frontsides |
        std::views::transform(&MPCFillCard::m_Name)
    };
    auto backsides{
        m_Set.m_Frontsides |
        std::views::transform(&MPCFillCard::m_Backside) |
        std::views::filter([](const auto& back)
                           { return back.has_value(); }) |
        std::views::transform([](const auto& back)
                              { return back.value().m_Name; })
    };
    std::vector<QString> files{
        "__back.png"
    };
    files.insert(files.end(), frontsides.begin(), frontsides.end());
    files.insert(files.end(), backsides.begin(), backsides.end());
    return files;
}

uint32_t MPCFillDownloader::GetAmount(const QString& file_name) const
{
    auto card{ std::ranges::find(m_Set.m_Frontsides, file_name, &MPCFillCard::m_Name) };
    if (card != m_Set.m_Frontsides.end())
    {
        return card->m_Amount;
    }
    return 0;
}

std::optional<QString> MPCFillDownloader::GetBackside(const QString& file_name) const
{
    auto card{ std::ranges::find(m_Set.m_Frontsides, file_name, &MPCFillCard::m_Name) };
    if (card != m_Set.m_Frontsides.end() && card->m_Backside.has_value())
    {
        return card->m_Backside.value().m_Name;
    }
    return std::nullopt;
}

std::vector<QString> MPCFillDownloader::GetDuplicates(const QString& file_name) const
{
    auto it{ m_Duplicates.find(file_name) };
    if (it != m_Duplicates.end())
    {
        return it->second;
    }
    return {};
}

bool MPCFillDownloader::ProvidesBleedEdge() const
{
    return true;
}

QString MPCFillDownloader::MPCFillIdFromUrl(const QString& url)
{
    return url.split("id=").back();
}

MPCFillDownloader::CardParseResult MPCFillDownloader::ParseMPCFillCard(const QDomElement& element)
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

bool MPCFillDownloader::PushSingleRequest()
{
    if (m_PendingRequests.empty())
    {
        return false;
    }

    auto [name, request_uri]{ m_PendingRequests.back() };
    m_PendingRequests.pop_back();

    LogInfo("Requesting card {}", name.toStdString());

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

    return true;
}
