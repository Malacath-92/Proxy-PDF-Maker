#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>

#include <ranges>

#include <fmt/ranges.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QThreadPool>
#include <QVBoxLayout>
#include <QWidget>

#include <ppp/app.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

#include <ppp/expiring_network_cache.hpp>

#include <ppp/plugins/mtg_card_downloader/mtg_card_browser_popup.hpp>
#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>
#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_settings.hpp>
#include <ppp/plugins/mtg_card_downloader/scryfall_endpoints.hpp>

#include <ppp/plugins/mtg_card_downloader/view_models/view_model_mtg_card_browser.hpp>

class MtGDownloaderPlugin : public PluginInterface
{
  public:
    MtGDownloaderPlugin(const QString& text,
                        Project& project,
                        const Config& config)
        : m_Project{ project }
        , m_Cfg{ config }
        , m_Widget{ new QWidget{} }
        , m_Button{ new QPushButton{ text } }
    {
        const auto cache_path{ ppApp->GetCacheFolder() / "mtg_cache" };
        auto* network_cache{ new FixedExpiryDiskCache };
        network_cache->setCacheDirectory(ToQString(cache_path));
        network_cache->setMaximumCacheSize(100 * 1024 * 1024);
        m_NetworkManager.setCache(network_cache);

        auto* layout{ new QVBoxLayout };
        layout->addWidget(m_Button);
        m_Widget->setLayout(layout);
        m_Widget->setObjectName("MtG Card Downloader");

        const auto open_downloader_popup{
            [this]()
            {
                m_Button->window()->setEnabled(false);
                AtScopeExit enable{ [this]
                                    { m_Button->window()->setEnabled(true); } };

                {
                    MtgDownloaderPopup downloader{
                        nullptr,
                        m_NetworkManager,
                        m_Project,
                        m_Cfg,
                        *this
                    };
                    downloader.Show();
                }
            }
        };

        QObject::connect(m_Button,
                         &QPushButton::clicked,
                         this,
                         open_downloader_popup);

        connect(&m_NetworkManager,
                &QNetworkAccessManager::sslErrors,
                this,
                [](QNetworkReply* reply, const QList<QSslError>& errors)
                {
                    auto error_strings{
                        errors |
                        std::views::transform([](QSslError error)
                                              { return error.errorString().toStdString(); })
                    };
                    LogError("SSL errors during request {}: {}",
                             reply->url().toString().toStdString(),
                             error_strings);
                });
    }
    virtual ~MtGDownloaderPlugin() override
    {
        for (auto* ext : m_CardExtensions)
        {
            ext->deleteLater();
        }
    }

    virtual QWidget* Widget() override
    {
        return m_Widget;
    }
    QPushButton* Button()
    {
        return m_Button;
    }

    virtual bool ProvidesWidgetExtension(PluginWidgetExtensionType type) const override
    { return type == PluginWidgetExtensionType::CardWidgetExtension; }
    virtual QWidget* MakeCardWidgetExtension(const fs::path& card_name) override
    {
        if (m_Project.GetCardMeta(card_name, "card_name") == std::nullopt)
        {
            return nullptr;
        }

        auto* ext{ new QPushButton{ "Browse" } };
        m_CardExtensions.push_back(ext);
        QObject::connect(ext,
                         &QObject::destroyed,
                         this,
                         [this](QObject* obj)
                         { std::erase(m_CardExtensions,
                                      static_cast<QWidget*>(obj)); });
        QObject::connect(ext,
                         &QPushButton::clicked,
                         this,
                         std::bind_front(&MtGDownloaderPlugin::ShowCardBrowser, this, std::cref(card_name)));
        return ext;
    }

  private:
    void ShowCardBrowser(const fs::path& card_name)
    {
        MtGCardBrowserPopup browser{
            nullptr,
            new MtGCardBrowserViewModel{
                m_Project,
                card_name,
                m_NetworkManager }
        };
        m_Button->window()->setEnabled(false);
        AtScopeExit enable{ [this]
                            { m_Button->window()->setEnabled(true); } };

        if (const auto card_uri{ browser.Show() })
        {
            if (const auto preview{ browser.GetSelectedPreview() })
            {
                auto it{ m_Project.m_CardSignallers.find(card_name) };
                if (it != m_Project.m_CardSignallers.end())
                {
                    it->second->PreviewUpdated(preview.value());
                }
            }

            const auto backside_name{ m_Project.GetBacksideAutoName(card_name) };
            const auto backside_uri{ browser.GetSelectedBacksideURI() };
            if (backside_uri.has_value())
            {
                m_Project.SetBacksideImage(card_name, backside_name);

                if (const auto backside_preview{ browser.GetSelectedBacksidePreview() })
                {
                    auto it{ m_Project.m_CardSignallers.find(backside_name) };
                    if (it != m_Project.m_CardSignallers.end())
                    {
                        it->second->PreviewUpdated(backside_preview.value());
                    }
                }
            }
            else
            {
                m_Project.SetBacksideImageDefault(card_name);
            }

            PauseCropper();
            AtScopeExit unpause{ [this]
                                 { UnpauseCropper(); } };
            PrimeCropper();

            auto scryfall_data_endpoint{ ScryfallDataEndpoint::Get(std::ref(m_NetworkManager)) };

            QEventLoop evt_loop{};
            auto download_image{
                [this, &evt_loop, scryfall_data_endpoint](const QString& card_uri, const fs::path& card_name)
                {
                    scryfall_data_endpoint->Call(
                        card_uri,
                        [this, &evt_loop, card_name](const QByteArray& image_data)
                        {
                            const auto settings{ MtgDownloaderSettings::Read() };

                            auto* worker{
                                new CardDownloaderImageWorker{
                                    m_Project,
                                    ToQString(card_name),
                                    image_data,
                                    settings.m_FillCorners,
                                    settings.m_UpscaleModel == "None"
                                        ? ""
                                        : settings.m_UpscaleModel,
                                    m_Project.CardSize(),
                                    m_Cfg.m_MaxDPI,
                                    { ToQString(m_Project.GetCardImagePath(card_name)) },
                                }
                            };
                            QObject::connect(worker,
                                             &CardDownloaderImageWorker::Done,
                                             &evt_loop,
                                             &QEventLoop::quit);
                            QThreadPool::globalInstance()->start(worker);
                        });
                }
            };
            download_image(card_uri.value(), card_name);
            if (backside_uri.has_value())
            {
                download_image(backside_uri.value(), backside_name);
            }
            evt_loop.exec();
        }
    }

    Project& m_Project;
    const Config& m_Cfg;

    QWidget* m_Widget;
    QPushButton* m_Button;

    std::vector<QWidget*> m_CardExtensions;

    QNetworkAccessManager m_NetworkManager;
};

PluginInterface* InitMtGCardDownloaderPlugin(Project& project, const Config& config)
{
    return new MtGDownloaderPlugin{ "Open", project, config };
}

void DestroyMtGCardDownloaderPlugin(PluginInterface* widget)
{
    delete widget;
}
