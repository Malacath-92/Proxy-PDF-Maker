#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader.hpp>

#include <QPushButton>

#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader_popup.hpp>

class YuGiOhDownloaderPlugin : public PluginInterface
{
  public:
    YuGiOhDownloaderPlugin(const QString& text)
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

PluginInterface* InitYuGiOhCardDownloaderPlugin(Project& project)
{
    auto* plugin{ new YuGiOhDownloaderPlugin{ "Open" } };
    auto* widget{ static_cast<QPushButton*>(plugin->Widget()) };
    widget->setObjectName("YuGiOh Card Downloader");

    const auto open_downloader_popup{
        [plugin, widget, &project]()
        {
            widget->window()->setEnabled(false);
            {
                YuGiOhDownloaderPopup downloader{ nullptr, project, *plugin };
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

void DestroyYuGiOhCardDownloaderPlugin(PluginInterface* widget)
{
    delete widget;
}
