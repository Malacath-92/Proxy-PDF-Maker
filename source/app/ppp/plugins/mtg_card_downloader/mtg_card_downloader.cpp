#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>

#include <QPushButton>

#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

QWidget* InitMtGCardDownloaderPlugin(Project& project)
{
    auto* widget{ new QPushButton{ "Open" } };
    widget->setObjectName("MtG Card Downloader");

    const auto open_downloader_popup{
        [widget, &project]()
        {
            MtgDownloaderPopup downloader{ nullptr, project };

            // QObject::connect(&downloader,
            //                  &PluginsPopup::PluginEnabled,
            //                  this,
            //                  &GlobalOptionsWidget::PluginEnabled);
            // QObject::connect(&downloader,
            //                  &PluginsPopup::PluginDisabled,
            //                  this,
            //                  &GlobalOptionsWidget::PluginDisabled);

            widget->window()->setEnabled(false);
            downloader.Show();
            widget->window()->setEnabled(true);
        }
    };

    QObject::connect(widget,
                     &QPushButton::clicked,
                     widget,
                     open_downloader_popup);

    return widget;
}

void DestroyMtGCardDownloaderPlugin(QWidget* widget)
{
    delete widget;
}
