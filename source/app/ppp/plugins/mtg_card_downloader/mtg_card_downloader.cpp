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
            widget->window()->setEnabled(false);
            {
                MtgDownloaderPopup downloader{ nullptr, project };
                downloader.Show();
            }
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
