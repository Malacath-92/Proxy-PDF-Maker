#pragma once

#include <memory>

#include <ppp/project/project_types.hpp>

#include <ppp/ui/view_models/view_model_selectable_card_grid.hpp>

class QNetworkAccessManager;

class Project;
class ProjectCardSignaller;

class ScryfallSearchEndpoint;
class ScryfallDataEndpoint;

class DownloadedMtGCardGridViewModel : public SelectableCardGridViewModel
{
    Q_OBJECT

  public:
    DownloadedMtGCardGridViewModel(const Project& project,
                                   const QString& query,
                                   QNetworkAccessManager& network_manager);
    ~DownloadedMtGCardGridViewModel();

    virtual bool HasCards() const override;
    virtual bool HasIgnoredCards() const override;

    virtual bool IsCardIgnored(const fs::path& card_name) const override;

    virtual CardViewModel* MakeCardViewModel(const fs::path& card_name) override;

    virtual const CardContainer& GetCards() const override;

    QString GetFullResImageURI(const QString& card_name) const;
    ImagePreview GetPreview(const QString& card_name) const;

  private:
    std::unordered_map<QString, ProjectCardSignaller> m_CardSignallers;
    std::unordered_map<QString, QString> m_PngURIs;
    std::unordered_map<QString, ImagePreview> m_Previews;

    std::shared_ptr<ScryfallSearchEndpoint> m_ScryfallSearch;
    std::shared_ptr<ScryfallDataEndpoint> m_ScryfallData;
};
