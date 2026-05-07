#pragma once

#include <memory>
#include <optional>

#include <QRunnable>
#include <QTemporaryDir>

#include <ppp/ui/popups.hpp>
#include <ppp/util.hpp>

#include <ppp/plugins/download_interface.hpp>

class QTextEdit;
class QProgressBar;
class QPushButton;
class QComboBox;
class QProgressBar;
class QNetworkAccessManager;
class QNetworkReply;

class Project;
class PluginInterface;
class DecklistTextEdit;

class CardDownloaderImageWorker : public QObject, public QRunnable
{
    Q_OBJECT

  public:
    CardDownloaderImageWorker(const Project& project,
                              QString image_name,
                              const QByteArray& image_data,
                              bool fill_corners,
                              QString upscale_model,
                              Size physical_card_size,
                              PixelDensity max_density,
                              std::vector<QString> out_files);

    virtual void run() override;

  signals:
    void Done();

  private:
    const Project& m_Project;
    QString m_ImageName;
    QByteArray m_ImageData;
    bool m_FillCorners;
    QString m_UpscaleModel;
    Size m_PhysicalCardSize;
    PixelDensity m_MaxDensity;
    std::vector<QString> m_OutFiles;
};

class CardDownloaderPopup : public PopupBase
{
    Q_OBJECT

  public:
    CardDownloaderPopup(QWidget* parent, Project& project, PluginInterface& router);
    virtual ~CardDownloaderPopup();

  protected slots:
    void DownloadProgress(int progress, int target);
    void ImageAvailable(const QByteArray& image_data, const QString& file_name);

  protected:
    virtual bool ClearImageFolder() const = 0;
    virtual bool DownloadBacksides() const = 0;
    virtual bool FillCorners() const = 0;
    virtual QString UpscaleModel() const = 0;

    virtual void TextChanged(const QString& text) = 0;
    virtual void PreDownload() = 0;
    virtual std::unique_ptr<CardArtDownloader> MakeDownloader(
        std::vector<QString> skip_files,
        std::optional<QString> backside_pattern) = 0;
    virtual void OnDownload() = 0;
    virtual void PostDownload() = 0;

    virtual void ValidateSettings() = 0;

    void StartDownload();
    void FinalizeDownload();

    void InstallLogHook();
    void UninstallLogHook();

    QString OutputDir() const;

    std::unique_ptr<QNetworkAccessManager> m_NetworkManager{ nullptr };

    DecklistTextEdit* m_TextInput{ nullptr };
    QLabel* m_Hint{ nullptr };
    QWidget* m_Upscale{ nullptr };
    QComboBox* m_UpscaleModel{ nullptr };
    QProgressBar* m_ProgressBar{ nullptr };
    QWidget* m_Buttons{ nullptr };
    QPushButton* m_DownloadButton{ nullptr };
    QPushButton* m_CancelButton{ nullptr };

    Project& m_Project;

    PluginInterface& m_Router;

  private:
    QTemporaryDir m_OutputDir{};

    uint32_t m_WaitingForImages{ 0 };
    uint32_t m_TotalImages{ 0 };
    bool m_DownloaderDone{ false };

    std::optional<uint32_t> m_LogHookId{ std::nullopt };

    std::unique_ptr<CardArtDownloader> m_Downloader;
};
