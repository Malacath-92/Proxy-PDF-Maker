#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <fmt/ranges.h>

#include <QApplication>
#include <QCheckBox>
#include <QDropEvent>
#include <QLabel>
#include <QMetaEnum>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QThreadPool>
#include <QVBoxLayout>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/upscale_models.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/widget_label.hpp>

#include <ppp/plugins/decklist_textbox.hpp>
#include <ppp/plugins/plugin_interface.hpp>
#include <ppp/plugins/yugioh_card_downloader/download_ygoprodeck.hpp>

YuGiOhDownloaderImageWorker::YuGiOhDownloaderImageWorker(const Project& project,
                                                         QString image_name,
                                                         QString out_file,
                                                         const QByteArray& image_data,
                                                         QString upscale_model,
                                                         Size physical_card_size,
                                                         PixelDensity max_density)
    : m_Project{ project }
    , m_ImageName{ std::move(image_name) }
    , m_OutFile{ std::move(out_file) }
    , m_ImageData{ image_data }
    , m_UpscaleModel{ std::move(upscale_model) }
    , m_PhysicalCardSize{ physical_card_size }
    , m_MaxDensity{ max_density }
{
}

void YuGiOhDownloaderImageWorker::run()
{
    if (!m_UpscaleModel.isEmpty())
    {
        LogInfo("Upscaling card image {}", m_ImageName.toStdString());

        auto image{
            Image::Decode(EncodedImageView{
                reinterpret_cast<const std::byte*>(m_ImageData.constData()),
                static_cast<size_t>(m_ImageData.size()),
            }),
        };

        auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
        auto upscaled_image{ RunModel(*app, m_UpscaleModel.toStdString(), image, m_PhysicalCardSize, m_MaxDensity) };

        const auto encoded_image{ upscaled_image.EncodePng() };
        m_ImageData.assign((const char*)encoded_image.data(), (const char*)encoded_image.data() + encoded_image.size());
    }

    {
        LogInfo("Writing image {}...", m_ImageName.toStdString());

        QFile file(m_OutFile);
        file.open(QIODevice::WriteOnly);
        file.write(m_ImageData);
    }

    Done();
}

YuGiOhDownloaderPopup::YuGiOhDownloaderPopup(QWidget* parent,
                                             Project& project,
                                             PluginInterface& router)
    : PopupBase{ parent }
    , m_Project{ project }
    , m_Router{ router }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);

    m_TextInput = new DecklistTextEdit;
    m_TextInput->setPlaceholderText("Paste decklist (YDK or YDKE)");

    m_Settings = new QCheckBox{ "Adjust Settings" };
    m_Settings->setChecked(true);

    m_ClearCheckbox = new QCheckBox{ "Clear Image Folder" };
    m_ClearCheckbox->setChecked(true);

    auto* upscale{ new ComboBoxWithLabel{ "Upscale", GetModelNames(), "None" } };
    m_UpscaleModel = upscale->GetWidget();

    m_Hint = new QLabel;
    m_Hint->setVisible(false);

    m_ProgressBar = new QProgressBar;
    m_ProgressBar->setTextVisible(true);
    m_ProgressBar->setVisible(false);
    m_ProgressBar->setFormat("%v/%m Requests");

    auto* buttons{ new QWidget{} };
    {
        m_DownloadButton = new QPushButton{ "Download" };
        m_CancelButton = new QPushButton{ "Cancel" };

        auto* layout{ new QHBoxLayout };
        layout->addWidget(m_DownloadButton);
        layout->addWidget(m_CancelButton);
        buttons->setLayout(layout);

        QObject::connect(m_DownloadButton,
                         &QPushButton::clicked,
                         this,
                         &YuGiOhDownloaderPopup::DoDownload);
        QObject::connect(m_CancelButton,
                         &QPushButton::clicked,
                         this,
                         &QDialog::close);
    }

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_TextInput);
    layout->addWidget(m_Settings);
    layout->addWidget(m_ClearCheckbox);
    layout->addWidget(upscale);
    layout->addWidget(m_Hint);
    layout->addWidget(m_ProgressBar);
    layout->addWidget(buttons);
    setLayout(layout);

    auto change_upscale_model{
        [this](const QString& t)
        {
            if (ModelRequiresDownload(t.toStdString()))
            {
                m_Hint->setVisible(true);
                m_Hint->setText("Upscale model will have to be downloaded.");
            }
            else
            {
                m_Hint->setVisible(false);
            }
        }
    };

    QObject::connect(m_UpscaleModel,
                     &QComboBox::currentTextChanged,
                     this,
                     change_upscale_model);

    m_OutputDir.setAutoRemove(true);

    m_Router.PauseCropper();
}

YuGiOhDownloaderPopup::~YuGiOhDownloaderPopup()
{
    m_Router.UnpauseCropper();
    UninstallLogHook();
}

void YuGiOhDownloaderPopup::DownloadProgress(int progress, int target)
{
    m_ProgressBar->setValue(progress);
    m_ProgressBar->setMaximum(target);

    if (progress == target)
    {
        m_DownloaderDone = true;
        if (m_WaitingForImages > 0)
        {
            m_ProgressBar->setFormat("%v/%m Images");
            m_ProgressBar->setMaximum(m_TotalImages);
            m_ProgressBar->setValue(m_TotalImages - m_WaitingForImages);
        }
        else
        {
            FinalizeDownload();
        }
    }
}

void YuGiOhDownloaderPopup::ImageAvailable(const QByteArray& image_data, const QString& file_name)
{
    LogInfo("Received data for card {}", file_name.toStdString());

    auto upscale_model{ m_UpscaleModel->currentText() };
    if (upscale_model == "None")
    {
        upscale_model.clear();
    }

    auto* worker{ new YuGiOhDownloaderImageWorker{
        m_Project,
        file_name,
        m_OutputDir.filePath(file_name),
        image_data,
        std::move(upscale_model),
        m_Project.CardSize(),
        g_Cfg.m_MaxDPI,
    } };
    QObject::connect(worker,
                     &YuGiOhDownloaderImageWorker::Done,
                     this,
                     [this]()
                     {
                         m_WaitingForImages--;
                         if (m_DownloaderDone)
                         {
                             m_ProgressBar->setValue(m_TotalImages - m_WaitingForImages);
                             if (m_WaitingForImages == 0)
                             {
                                 FinalizeDownload();
                             }
                         }
                     });

    m_WaitingForImages++;
    m_TotalImages++;
    QThreadPool::globalInstance()->start(worker);
}

void YuGiOhDownloaderPopup::DoDownload()
{
    ValidateSettings();

    m_NetworkManager = std::make_unique<QNetworkAccessManager>(this);
    InstallLogHook();

    const auto upscale_model{ m_UpscaleModel->currentText().toStdString() };
    if (upscale_model != "None")
    {
        if (ModelRequiresDownload(upscale_model))
        {
            if (auto url{ GetModelUrl(upscale_model) })
            {
                LogInfo("Downloading model {}", upscale_model);
                m_ProgressBar->setVisible(true);

                QNetworkRequest get_request{ ToQString(url.value()) };
                QNetworkReply* reply{ m_NetworkManager->get(std::move(get_request)) };

                QObject::connect(reply,
                                 &QNetworkReply::finished,
                                 this,
                                 [this, upscale_model, reply]()
                                 {
                                     {
                                         QFile file(ToQString(GetModelFilename(upscale_model)));
                                         file.open(QIODevice::WriteOnly);
                                         file.write(reply->readAll());
                                     }

                                     m_ProgressBar->setVisible(false);
                                     reply->deleteLater();

                                     // Initiate the download again
                                     DoDownload();
                                 });
                QObject::connect(reply,
                                 &QNetworkReply::downloadProgress,
                                 this,
                                 [this](qint64 bytes_received, qint64 bytes_total)
                                 {
                                     m_ProgressBar->setValue(static_cast<int>(bytes_received));
                                     m_ProgressBar->setMaximum(static_cast<int>(bytes_total));
                                 });
                QObject::connect(reply,
                                 &QNetworkReply::errorOccurred,
                                 reply,
                                 [](QNetworkReply::NetworkError error)
                                 {
                                     LogError("Failed downloading model: {}",
                                              QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error));
                                 });

                // Quit here, we reinitiate once we are done downloading
                return;
            }
        }

        LogInfo("Preloading upscaling model...");
        auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
        LoadModel(*app, upscale_model);
    }

    connect(m_NetworkManager.get(),
            &QNetworkAccessManager::sslErrors,
            this,
            [](QNetworkReply* reply, const QList<QSslError>& errors)
            {
                auto error_strings{
                    errors |
                    std::views::transform([](QSslError error)
                                          { return error.errorString().toStdString(); })
                };
                LogError("SSL errors during request {}: {}",
                         reply->url().toString().toStdString(),
                         error_strings);
            });

    const bool clear_image_folder{ m_ClearCheckbox->isChecked() };
    const auto skip_images{
        (!clear_image_folder
             ? ListImageFiles(m_Project.m_Data.m_ImageDir, m_Project.m_Data.m_CropDir)
             : std::vector<fs::path>{}) |
        std::views::transform(static_cast<QString (*)(const fs::path&)>(&ToQString)) |
        std::ranges::to<std::vector>()
    };

    LogInfo("Downloading YuGiOh Decklist to {}",
            m_OutputDir.path().toStdString());
    m_Downloader = std::make_unique<YGOProDeckDownloader>(
        std::move(skip_images));

    if (m_Downloader->ParseInput(m_TextInput->toPlainText()))
    {
        connect(m_NetworkManager.get(),
                &QNetworkAccessManager::finished,
                m_Downloader.get(),
                &CardArtDownloader::HandleReply);

        connect(m_Downloader.get(),
                &CardArtDownloader::Progress,
                this,
                &YuGiOhDownloaderPopup::DownloadProgress);
        connect(m_Downloader.get(),
                &CardArtDownloader::ImageAvailable,
                this,
                &YuGiOhDownloaderPopup::ImageAvailable);

        if (m_Downloader->BeginDownload(*m_NetworkManager))
        {
            m_ProgressBar->setVisible(true);
        }
        else
        {
            LogError("Failed initializing download...");
        }
    }
    else
    {
        LogError("Failed parsing input file...");
    }

    m_DownloadButton->setDisabled(true);
    m_Settings->setDisabled(true);
    m_ClearCheckbox->setDisabled(true);
    m_UpscaleModel->setDisabled(true);
}

void YuGiOhDownloaderPopup::FinalizeDownload()
{
    const auto upscale_model{ m_UpscaleModel->currentText().toStdString() };
    if (upscale_model != "None")
    {
        auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
        UnloadModel(*app, upscale_model);
    }

    const auto downloaded_files{
        m_Downloader->GetFiles()
    };
    const auto downloaded_file_paths{
        downloaded_files |
        std::views::transform([](const auto& file)
                              { return fs::path{ file.toStdString() }; }) |
        std::ranges::to<std::vector>()
    };

    const auto& target_dir{
        m_Project.m_Data.m_ImageDir
    };
    if (!fs::exists(target_dir))
    {
        fs::create_directories(target_dir);
    }

    const bool clear_image_folder{ m_ClearCheckbox->isChecked() };
    if (clear_image_folder)
    {
        std::vector<fs::path> cards_to_remove;
        for (const auto& card_info : m_Project.m_Data.m_Cards)
        {
            if (card_info.m_Name == m_Project.m_Data.m_BacksideDefault)
            {
                continue;
            }

            if (!card_info.m_ExternalPath.has_value() &&
                !std::ranges::contains(downloaded_file_paths, card_info.m_Name) &&
                fs::exists(target_dir / card_info.m_Name))
            {
                fs::remove(target_dir / card_info.m_Name);
            }

            cards_to_remove.push_back(card_info.m_Name);
        }

        for (const auto& card_name : cards_to_remove)
        {
            m_Project.CardRemoved(card_name);
        }
    }

    const fs::path output_dir{
        m_OutputDir.path().toStdString()
    };

    for (const auto& [card, path] : std::views::zip(downloaded_files, downloaded_file_paths))
    {
        if (fs::exists(output_dir / path))
        {
            fs::rename(output_dir / path, target_dir / path);
        }

        m_Project.CardAdded(path);
        m_Project.SetCardCount(path, m_Downloader->GetAmount(card));

        m_Project.SetBacksideImageDefault(path);
    }

    m_Router.RefreshCardGrid();
    m_Router.UnpauseCropper();

    m_CancelButton->setText("Close");

    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
    main_window->Toast(ToastType::Info,
                       "MtG Download done",
                       "All files have been downloaded, you can now close the plugin.");
}

void YuGiOhDownloaderPopup::InstallLogHook()
{
    UninstallLogHook();

    // clang-format off
    m_LogHookId = Log::GetInstance(Log::c_MainLogName)->InstallHook(
        [this](const Log::DetailInformation&, Log::LogLevel, std::string_view message)
        {
            m_Hint->setVisible(true);
            m_Hint->setText(ToQString(message));
        });
    // clang-format on
}

void YuGiOhDownloaderPopup::UninstallLogHook()
{
    if (m_LogHookId.has_value())
    {
        Log::GetInstance(Log::c_MainLogName)->UninstallHook(m_LogHookId.value());
    }
}

void YuGiOhDownloaderPopup::ValidateSettings()
{
    if (!m_Settings->isChecked())
    {
        return;
    }

    if (m_Project.m_Data.m_CardSizeChoice != "Japanese")
    {
        m_Router.SetCardSizeChoice("Japanese");
    }

    m_Router.SetEnableBackside(m_Project.HasValidDefaultBackside());

    if (m_Project.m_Data.m_BacksideAutoPattern.empty())
    {
        m_Router.SetBacksideAutoPattern("__back_$");
    }
}
