#pragma once

#include <any>
#include <memory>
#include <optional>

#include <QTemporaryDir>

#include <ppp/plugins/mtg_card_downloader/download_interface.hpp>
#include <ppp/ui/popups.hpp>

class QCheckBox;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;

class Project;
class PluginInterface;

enum class InputType
{
    Decklist,
    ScryfallQuery,
    MPCAutofill,
    None,
};

class MtgDownloaderPopup : public PopupBase
{
  public:
    MtgDownloaderPopup(QWidget* parent, Project& project, PluginInterface& router);
    ~MtgDownloaderPopup();

  private slots:
    void DownloadProgress(int progress, int target);
    void ImageAvailable(const QByteArray& image_data, const QString& file_name);

  private:
    void DoDownload();
    void FinalizeDownload();

    void InstallLogHook();
    void UninstallLogHook();

    void ValidateSettings();

    static InputType StupidInferSource(const QString& text);

    Project& m_Project;

    PluginInterface& m_Router;

    InputType m_InputType{ InputType::None };

    QTextEdit* m_TextInput{ nullptr };
    QCheckBox* m_Settings{ nullptr };
    QCheckBox* m_Backsides{ nullptr };
    QCheckBox* m_ClearCheckbox{ nullptr };
    QCheckBox* m_FillCornersCheckbox{ nullptr };
    QLabel* m_Hint{ nullptr };
    QProgressBar* m_ProgressBar{ nullptr };
    QPushButton* m_DownloadButton{ nullptr };
    QPushButton* m_CancelButton{ nullptr };

    QTemporaryDir m_OutputDir{};

    std::optional<uint32_t> m_LogHookId{ std::nullopt };

    std::unique_ptr<CardArtDownloader> m_Downloader;
    std::unique_ptr<QNetworkAccessManager> m_NetworkManager;
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
