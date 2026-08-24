#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

class DownloaderPlugin : public PluginInterface
{
  public:
    DownloaderPlugin(const QString& text)
        : m_Widget{ new QWidget{} }
        , m_Button{ new QPushButton{ text } }
    {
        auto* layout{ new QVBoxLayout };
        layout->addWidget(m_Button);
        m_Widget->setLayout(layout);
        m_Widget->setObjectName("MtG Card Downloader");
    }

    virtual QWidget* Widget() override
    {
        return m_Widget;
    }
    QPushButton* Button()
    {
        return m_Button;
    }

  private:
    QWidget* m_Widget;
    QPushButton* m_Button;
};

PluginInterface* InitMtGCardDownloaderPlugin(Project& project, const Config& config)
{
    auto* plugin{ new DownloaderPlugin{ "Open" } };
    auto* button{ plugin->Button() };

    const auto open_downloader_popup{
        [plugin, button, &project, &config]()
        {
            button->window()->setEnabled(false);
            {
                MtgDownloaderPopup downloader{ nullptr, project, config, *plugin };
                downloader.Show();
            }
            button->window()->setEnabled(true);
        }
    };

    QObject::connect(button,
                     &QPushButton::clicked,
                     button,
                     open_downloader_popup);

    return plugin;
}

void DestroyMtGCardDownloaderPlugin(PluginInterface* widget)
{
    delete widget;
}
