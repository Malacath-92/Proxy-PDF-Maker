#pragma once

#include <any>
#include <memory>
#include <optional>

#include <QRunnable>
#include <QTemporaryDir>

#include <ppp/plugins/download_interface.hpp>
#include <ppp/ui/popups.hpp>
#include <ppp/util.hpp>

class QCheckBox;
class QComboBox;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;

class Project;
class PluginInterface;

class YuGiOhDownloaderImageWorker : public QObject, public QRunnable
{
    Q_OBJECT

  public:
    YuGiOhDownloaderImageWorker(const Project& project,
                                QString image_name,
                                QString out_file,
                                const QByteArray& image_data,
                                QString upscale_model,
                                Size physical_card_size,
                                PixelDensity max_density);

    virtual void run() override;

  signals:
    void Done();

  private:
    const Project& m_Project;
    QString m_ImageName;
    QString m_OutFile;
    QByteArray m_ImageData;
    QString m_UpscaleModel;
    Size m_PhysicalCardSize;
    PixelDensity m_MaxDensity;
};

class YuGiOhDownloaderPopup : public PopupBase
{
    Q_OBJECT

  public:
    YuGiOhDownloaderPopup(QWidget* parent, Project& project, PluginInterface& router);
    ~YuGiOhDownloaderPopup();

  private slots:
    void DownloadProgress(int progress, int target);
    void ImageAvailable(const QByteArray& image_data, const QString& file_name);

  private:
    void DoDownload();
    void FinalizeDownload();

    void InstallLogHook();
    void UninstallLogHook();

    void ValidateSettings();

    Project& m_Project;

    PluginInterface& m_Router;

    QTextEdit* m_TextInput{ nullptr };
    QCheckBox* m_Settings{ nullptr };
    QCheckBox* m_ClearCheckbox{ nullptr };
    QComboBox* m_UpscaleModel{ nullptr };
    QLabel* m_Hint{ nullptr };
    QProgressBar* m_ProgressBar{ nullptr };
    QPushButton* m_DownloadButton{ nullptr };
    QPushButton* m_CancelButton{ nullptr };

    QTemporaryDir m_OutputDir{};

    uint32_t m_WaitingForImages{ 0 };
    uint32_t m_TotalImages{ 0 };
    bool m_DownloaderDone{ false };

    std::optional<uint32_t> m_LogHookId{ std::nullopt };

    std::unique_ptr<CardArtDownloader> m_Downloader;
    std::unique_ptr<QNetworkAccessManager> m_NetworkManager;
};
