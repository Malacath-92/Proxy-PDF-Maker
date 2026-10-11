#include <ppp/plugins/mtg_card_downloader/view_models/view_model_mtg_card_browser.hpp>

#include <ranges>

#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/plugins/mtg_card_downloader/view_models/view_model_downloaded_mtg_card_grid.hpp>

#include <ppp/profile/profile.hpp>

MtGCardBrowserViewModel::MtGCardBrowserViewModel(const Project& project,
                                                 const fs::path& card_name,
                                                 QNetworkAccessManager& network_manager)
    : m_Project{ project }
    , m_CardName{ card_name }
    , m_NetworkManager{ network_manager }
{
}

DownloadedMtGCardGridViewModel* MtGCardBrowserViewModel::MakeGridViewModel()
{
    return new DownloadedMtGCardGridViewModel{ m_Project, GetQuery(), m_NetworkManager };
}

QString MtGCardBrowserViewModel::GetQuery() const
{
    if (auto orig_card_name{ m_Project.GetCardMeta(m_CardName, "card_name") })
    {
        return QString{ "@@ !\"%1\" sort:released prefer:finish:nonfoil" }
            .arg(ToQString(orig_card_name.value()));
    }
    return QString{ "@@ !\"%1\" sort:released prefer:finish:nonfoil" }
        .arg(ToQString(m_CardName));
}
