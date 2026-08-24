#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader.hpp>

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader_popup.hpp>

class YuGiOhDownloaderPlugin : public PluginInterface
{
  public:
    YuGiOhDownloaderPlugin(const QString& text)
        : m_Widget{ new QWidget{} }
        , m_Button{ new QPushButton{ text } }
    {
        auto* layout{ new QVBoxLayout };
        layout->addWidget(m_Button);
        m_Widget->setLayout(layout);
        m_Widget->setObjectName("YuGiOh Card Downloader");
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

PluginInterface* InitYuGiOhCardDownloaderPlugin(Project& project, const Config& config)
{
    auto* plugin{ new YuGiOhDownloaderPlugin{ "Open" } };
    auto* button{ plugin->Button() };

    const auto open_downloader_popup{
        [plugin, button, &project, &config]()
        {
            button->window()->setEnabled(false);
            {
                YuGiOhDownloaderPopup downloader{ nullptr, project, config, *plugin };
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

void DestroyYuGiOhCardDownloaderPlugin(PluginInterface* widget)
{
    delete widget;
}
