#include <ppp/plugins/mtg_card_downloader/download_mpcfill.hpp>

#include <QDomDocument>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <ppp/image.hpp>
#include <ppp/util/log.hpp>

MPCFillCard ParseMPCFillCard(const QDomElement& element)
{
    auto name{ element.firstChildElement("name").text() };
    auto id{ element.firstChildElement("id").text() };
    auto amount{ element.firstChildElement("slots").text().count(",") };

    return MPCFillCard{
        std::move(name),
        std::move(id),
        static_cast<uint32_t>(amount)
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

    QDomElement fronts{ order.firstChildElement("fronts") };
    if (fronts.isNull())
    {
        LogError("No fronts are defined in MPCFill xml");
        return std::nullopt;
    }

    const auto cards{ fronts.childNodes() };
    set.m_Frontsides.reserve(cards.count());

    for (int i = 0; i < cards.count(); i++)
    {
        const auto& card{ cards.item(i).toElement() };
        set.m_Frontsides.push_back(ParseMPCFillCard(card));
    }

    QDomElement backside{ order.firstChildElement("cardback") };
    if (backside.isNull())
    {
        LogError("No cardback is defined in MPCFill xml");
        return std::nullopt;
    }

    set.m_BacksideId = backside.text();

    return set;
}

bool BeginDownloadMPCFill(QNetworkAccessManager& network_manager,
                          const MPCFillSet& set)
{
    auto do_download{
        [&network_manager](const QString& name, const QString& id)
        {
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
                                          reply->url().toString().toStdString(),
                                          QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
                             });
        }
    };

    for (const auto& card : set.m_Frontsides)
    {
        do_download(card.m_Name, card.m_Id);
    }
    do_download("__back.png", set.m_BacksideId);

    return true;
}

QString MPCFillIdFromUrl(const QString& url)
{
    return url.split("id=").back();
}

QByteArray ImageDataFromReply(const QByteArray& reply)
{
    return QByteArray::fromBase64(reply);
}

Image ImageFromReply(const QByteArray& reply)
{
    const auto decoded_data{ ImageDataFromReply(reply) };
    return Image::Decode(EncodedImageView{
        reinterpret_cast<const std::byte*>(decoded_data.data()),
        static_cast<size_t>(decoded_data.size()),
    });
}
