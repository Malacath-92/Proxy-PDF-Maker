#pragma once

#include <ppp/plugins/card_downloader_popup.hpp>
#include <ppp/plugins/download_interface.hpp>

class QCheckBox;
class QNetworkAccessManager;

class YuGiOhDownloaderPopup : public CardDownloaderPopup
{
    Q_OBJECT

  public:
    YuGiOhDownloaderPopup(QWidget* parent, Project& project, PluginInterface& router);

  private:
    virtual bool ClearImageFolder() const override;
    virtual bool DownloadBacksides() const override;
    virtual bool FillCorners() const override;
    virtual QString UpscaleModel() const override;

    virtual void TextChanged(const QString& /*text*/) override;
    virtual void PreDownload() override;
    virtual std::unique_ptr<CardArtDownloader> MakeDownloader(
        std::vector<QString> skip_files,
        std::optional<QString> backside_pattern) override;
    virtual void OnDownload() override;
    virtual void PostDownload() override;

    virtual void ValidateSettings() override;

    QCheckBox* m_Settings{ nullptr };
    QCheckBox* m_ClearCheckbox{ nullptr };
};
