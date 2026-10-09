#include <ppp/plugins/mtg_card_downloader/view_models/view_model_downloaded_mtg_card_grid.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/plugins/mtg_card_downloader/scryfall_endpoints.hpp>

#include <ppp/ui/view_models/view_model_card.hpp>

DownloadedMtGCardGridViewModel::DownloadedMtGCardGridViewModel(const Project& project,
                                                               const fs::path& card_name,
                                                               QNetworkAccessManager& network_manager)
    : SelectableCardGridViewModel{ project, {} }
{
    if (auto orig_card_name{ project.GetCardMeta(card_name, "card_name") })
    {
        m_ScryfallSearch = ScryfallSearchEndpoint::Get(std::ref(network_manager));

        m_ScryfallSearch->Queue(
            QString{ "@@ !\"%1\" sort:released prefer:finish:nonfoil" }.arg(ToQString(orig_card_name.value())),
            [&, this](const QJsonDocument& doc)
            {
                // Using curly-braces for initializers here makes gcc and clang
                // create an array-of-array, hence we are forced to use parens
                const auto data(doc["data"].toArray());

                if (data.size() > 0)
                {
                    m_ScryfallData = ScryfallDataEndpoint::Get(std::ref(network_manager));
                }

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
                    auto card_file_name{ QString{ "%1 - %2 (%3)" }
                                             .arg(card_name)
                                             .arg(card_set.toUpper())
                                             .arg(card_collector_number.rightJustified(4, '0'))
                                             .replace(QRegularExpression{ "[/\\:*?\"<>|]" }, "_") };
                    CardAdded(card_file_name.toStdString());

                    auto on_image{
                        [&, card_file_name](const QByteArray& image_data)
                        {
                            auto image{
                                Image::Decode(EncodedImageView{
                                    reinterpret_cast<const std::byte*>(image_data.constData()),
                                    static_cast<size_t>(image_data.size()),
                                }),
                            };
                            // image = image.RoundCorners(m_Project.CardSize(), m_Project.CardCornerRadius());
                            m_CardSignallers[card_file_name].PreviewUpdated(ImagePreview{
                                .m_UncroppedImage{},
                                .m_CroppedImage{ std::move(image) },
                                .m_BadAspectRatio = false,
                                .m_BadRotation = false,
                            });
                        }
                    };

                    auto download_image{
                        [this, &on_image](const QJsonObject& json)
                        {
                            const auto image_uris{ json["image_uris"].toObject() };
                            if (image_uris.contains("normal"))
                            {
                                m_ScryfallData->Call(image_uris["normal"].toString(), on_image);
                            }
                            else if (image_uris.contains("grid"))
                            {
                                m_ScryfallData->Call(image_uris["grid"].toString(), on_image);
                            }
                            else if (image_uris.contains("display"))
                            {
                                m_ScryfallData->Call(image_uris["display"].toString(), on_image);
                            }
                        }
                    };

                    if (card_obj.contains("card_faces"))
                    {
                        download_image(card_obj["card_faces"][0].toObject());
                    }
                    else
                    {
                        download_image(card_obj);
                    }
                }
            });
    }

    // TODO: Double-sided cards
}
DownloadedMtGCardGridViewModel::~DownloadedMtGCardGridViewModel() = default;

bool DownloadedMtGCardGridViewModel::HasCards() const
{
    // This is only initial cards
    return false;
}
bool DownloadedMtGCardGridViewModel::HasIgnoredCards() const
{
    return false;
}

bool DownloadedMtGCardGridViewModel::IsCardIgnored(const fs::path& /* card_name */) const
{
    return false;
}

CardViewModel* DownloadedMtGCardGridViewModel::MakeCardViewModel(const fs::path& card_name)
{
    auto* card_view_model{ SelectableCardGridViewModel::MakeCardViewModel(card_name) };
    QObject::connect(&m_CardSignallers[ToQString(card_name)],
                     &ProjectCardSignaller::PreviewUpdated,
                     card_view_model,
                     &CardViewModel::PreviewUpdated);
    return card_view_model;
}

const CardContainer& DownloadedMtGCardGridViewModel::GetCards() const
{
    // This is only initial cards
    static const CardContainer c_NoCards;
    return c_NoCards;
}
