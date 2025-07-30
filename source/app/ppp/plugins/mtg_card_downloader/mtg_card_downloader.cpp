#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>

#include <QPushButton>

#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

class DownloaderPlugin : public PluginInterface
{
  public:
    DownloaderPlugin(const QString& text)
        : m_Button{ new QPushButton{ text } }
    {
    }

    virtual QPushButton* Widget() override
    {
        return m_Button;
    }

  private:
    QPushButton* m_Button;
};

PluginInterface* InitMtGCardDownloaderPlugin(Project& project)
{
    auto* plugin{ new DownloaderPlugin{ "Open" } };
    auto* widget{ static_cast<QPushButton*>(plugin->Widget()) };
    widget->setObjectName("MtG Card Downloader");

    const auto open_downloader_popup{
        [plugin, widget, &project]()
        {
            widget->window()->setEnabled(false);
            {
                MtgDownloaderPopup downloader{ nullptr, project, *plugin };
                downloader.Show();
            }
            widget->window()->setEnabled(true);
        }
    };

    QObject::connect(widget,
                     &QPushButton::clicked,
                     widget,
                     open_downloader_popup);

    return plugin;
}

void DestroyMtGCardDownloaderPlugin(PluginInterface* widget)
{
    delete widget;
}
