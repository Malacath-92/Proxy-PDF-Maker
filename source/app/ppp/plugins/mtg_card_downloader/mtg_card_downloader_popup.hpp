#pragma once

#include <ppp/plugins/card_downloader_popup.hpp>

class QCheckBox;
class QNetworkAccessManager;

enum class InputType
{
    Decklist,
    ScryfallQuery,
    MPCAutofill,
    None,
};

class MtgDownloaderPopup : public CardDownloaderPopup
{
    Q_OBJECT

  public:
    MtgDownloaderPopup(QWidget* parent, Project& project, PluginInterface& router);

  private:
    virtual bool ClearImageFolder() const override;
    virtual bool DownloadBacksides() const override;
    virtual bool FillCorners() const override;
    virtual QString UpscaleModel() const override;

    virtual void TextChanged(const QString& text) override;
    virtual void PreDownload() override;
    virtual std::unique_ptr<CardArtDownloader> MakeDownloader(
        std::vector<QString> skip_files,
        std::optional<QString> backside_pattern) override;
    virtual void OnDownload() override;
    virtual void PostDownload() override;

    virtual void ValidateSettings() override;

    static InputType StupidInferSource(const QString& text);

    InputType m_InputType{ InputType::None };

    QCheckBox* m_Settings{ nullptr };
    QCheckBox* m_Backsides{ nullptr };
    QCheckBox* m_ClearCheckbox{ nullptr };
    QCheckBox* m_FillCornersCheckbox{ nullptr };
};

class SelectInputTypePopup : public PopupBase
{
  public:
    SelectInputTypePopup(QWidget* parent);

    InputType GetInputType() const
    {
        return m_InputType;
    }

  private:
    InputType m_InputType{ InputType::None };
};
