#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>

#include <ranges>

#include <fmt/ranges.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <ppp/util/log.hpp>

#include <ppp/project/project.hpp>

#include <ppp/plugins/mtg_card_downloader/mtg_card_browser_popup.hpp>
#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

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
        auto* layout{ new QVBoxLayout };
        layout->addWidget(m_Button);
        m_Widget->setLayout(layout);
        m_Widget->setObjectName("MtG Card Downloader");

        const auto open_downloader_popup{
            [this]()
            {
                m_Button->window()->setEnabled(false);
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
                m_Button->window()->setEnabled(true);
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
                         &QPushButton::pressed,
                         this,
                         [this, card_name]()
                         {
                             MtGCardBrowserPopup browser{
                                 nullptr,
                                 new MtGCardBrowserViewModel{
                                     m_Project,
                                     card_name,
                                     m_NetworkManager }
                             };
                             browser.Show();
                         });
        return ext;
    }

  private:
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
