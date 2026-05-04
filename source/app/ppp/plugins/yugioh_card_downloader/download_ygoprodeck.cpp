#include <ppp/plugins/yugioh_card_downloader/download_ygoprodeck.hpp>

#include <ranges>

#include <QNetworkReply>
#include <QRegularExpression>

#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

#include <ppp/util/log.hpp>

YGOProDeckDownloader::YGOProDeckDownloader(std::vector<QString> skip_files)
    : CardArtDownloader{ std::move(skip_files), std::nullopt }
{
    m_Timer.setSingleShot(true);
    m_Timer.setInterval(50);

    QObject::connect(&m_Timer,
                     &QTimer::timeout,
                     this,
                     &YGOProDeckDownloader::NextRequest);
}
YGOProDeckDownloader::~YGOProDeckDownloader() = default;

bool YGOProDeckDownloader::ParseInput(const QString& input)
{
    const QString ydke_uri_start{ "ydke://" };

    const auto lines{
        input.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
    };
    for (const auto& line : lines)
    {
        if (line.startsWith(ydke_uri_start))
        {
            const auto boards{
                line
                    .sliced(ydke_uri_start.size())
                    .split("!", Qt::SplitBehaviorFlags::SkipEmptyParts)
            };
            for (const auto& base64_board : boards)
            {
                const auto decoding{ QByteArray::fromBase64Encoding(base64_board.toUtf8()) };
                QDataStream stream{ decoding.decoded };
                stream.setByteOrder(QDataStream::LittleEndian);
                while (!stream.atEnd())
                {
                    uint32_t id{ 0 };
                    stream >> id;
                    if (id != 0)
                    {
                        m_Cards[id].m_Amount++;
                    }
                }
            }
        }
        else
        {
            bool ok;
            const uint32_t id{ line.toUInt(&ok) };
            if (ok)
            {
                m_Cards[id].m_Amount++;
            }
        }
    }

    for (auto& [id, info] : m_Cards)
    {
        info.m_FileName = QString{ "%1.jpg" }.arg(id);
        m_FileNameIdMap[info.m_FileName] = id;
    }
    m_CardIds = m_Cards | std::views::keys | std::ranges::to<std::vector>();

    return true;
}

bool YGOProDeckDownloader::BeginDownload(QNetworkAccessManager& network_manager)
{
    m_NetworkManager = &network_manager;

    m_TotalRequests = static_cast<uint32_t>(m_Cards.size());
    NextRequest();

    Progress(0, static_cast<int>(m_TotalRequests));

    return true;
}

void YGOProDeckDownloader::HandleReply(QNetworkReply* reply)
{
    const auto& id{ m_CardIds[m_Progress] };
    ImageAvailable(reply->readAll(), m_Cards.at(id).m_FileName);

    ++m_Progress;
    Progress(static_cast<int>(m_Progress), static_cast<int>(m_TotalRequests));

    if (m_Progress != m_TotalRequests)
    {
        m_Timer.start();
    }

    reply->deleteLater();
}

std::vector<QString> YGOProDeckDownloader::GetFiles() const
{
    auto cards{ m_Cards |
                std::views::values |
                std::views::transform(&CardInfo::m_FileName) |
                std::ranges::to<std::vector>() };
    return cards;
}

uint32_t YGOProDeckDownloader::GetAmount(const QString& file_name) const
{
    if (!m_FileNameIdMap.contains(file_name))
    {
        return 0;
    }

    return m_Cards.at(m_FileNameIdMap.at(file_name)).m_Amount;
}

std::optional<QString> YGOProDeckDownloader::GetBackside(const QString& /*file_name*/) const
{
    return std::nullopt;
}

std::vector<QString> YGOProDeckDownloader::GetDuplicates(const QString& /*file_name*/) const
{
    return {};
}

bool YGOProDeckDownloader::ProvidesBleedEdge() const
{
    return false;
}

void YGOProDeckDownloader::NextRequest()
{
    const auto request_uri{
        QString{ "https://images.ygoprodeck.com/images/cards/%1.jpg" }
            .arg(m_CardIds[m_Progress])
    };

    QNetworkRequest request{ std::move(request_uri) };
    request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                      ToQString(fmt::format("Proxy-PDF-Maker/{}", ProxyPdfVersion())));
    request.setRawHeader("Accept",
                         "*/*");

    m_NetworkManager->get(std::move(request));
}
