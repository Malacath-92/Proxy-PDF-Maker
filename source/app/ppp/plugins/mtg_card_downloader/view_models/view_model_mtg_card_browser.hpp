#pragma once

#include <span>

#include <QObject>

#include <ppp/project/project_types.hpp>

class QNetworkAccessManager;

class SelectableCardGridViewModel;

class Project;

class MtGCardBrowserViewModel : public QObject
{
    Q_OBJECT

  public:
    MtGCardBrowserViewModel(const Project& project,
                            const fs::path& card_name,
                            QNetworkAccessManager& network_manager);

    SelectableCardGridViewModel* MakeGridViewModel();

  private:
    const Project& m_Project;
    const fs::path& m_CardName;
    QNetworkAccessManager& m_NetworkManager;
};
