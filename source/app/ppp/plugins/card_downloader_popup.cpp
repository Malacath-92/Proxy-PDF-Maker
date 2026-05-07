#include <ppp/plugins/card_downloader_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <fmt/ranges.h>

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QThreadPool>

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

CardDownloaderImageWorker::CardDownloaderImageWorker(const Project& project,
                                                     QString image_name,
                                                     const QByteArray& image_data,
                                                     bool fill_corners,
                                                     QString upscale_model,
                                                     Size physical_card_size,
                                                     PixelDensity max_density,
                                                     std::vector<QString> out_files)
    : m_Project{ project }
    , m_ImageName{ std::move(image_name) }
    , m_ImageData{ image_data }
    , m_FillCorners{ fill_corners }
    , m_UpscaleModel{ std::move(upscale_model) }
    , m_PhysicalCardSize{ physical_card_size }
    , m_MaxDensity{ max_density }
    , m_OutFiles{ std::move(out_files) }
{
}

void CardDownloaderImageWorker::run()
{
    if (m_FillCorners)
    {
        LogInfo("Filling corners of image {}...", m_ImageName.toStdString());

        auto image{
            Image::Decode(EncodedImageView{
                reinterpret_cast<const std::byte*>(m_ImageData.constData()),
                static_cast<size_t>(m_ImageData.size()),
            }),
        };
        const auto image_with_corners{
            image.FillCorners(m_Project.CardSize(), m_Project.CardCornerRadius() * 1.65f)
        };
        const auto encoded_image{ image_with_corners.EncodePng() };
        m_ImageData.assign((const char*)encoded_image.data(), (const char*)encoded_image.data() + encoded_image.size());
    }

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

    for (const auto& out_file : m_OutFiles)
    {
        LogInfo("Writing image {}...", m_ImageName.toStdString());

        QFile file(out_file);
        file.open(QIODevice::WriteOnly);
        file.write(m_ImageData);
    }

    Done();
}

CardDownloaderPopup::CardDownloaderPopup(QWidget* parent,
                                         Project& project,
                                         PluginInterface& router)
    : PopupBase{ parent }
    , m_Project{ project }
    , m_Router{ router }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);

    m_TextInput = new DecklistTextEdit;
    m_TextInput->setPlaceholderText("Paste decklist (Moxfield, Archidekt, MODO, or MTGA), MPC Autofill xml, or a Scryfall query prepended with $");

    m_Hint = new QLabel;
    m_Hint->setVisible(false);

    auto* upscale{ new ComboBoxWithLabel{ "Upscale", GetModelNames(), "None" } };
    m_Upscale = upscale;
    m_UpscaleModel = upscale->GetWidget();

    m_ProgressBar = new QProgressBar;
    m_ProgressBar->setTextVisible(true);
    m_ProgressBar->setVisible(false);
    m_ProgressBar->setFormat("%v/%m Requests");

    m_Buttons = new QWidget;
    {
        m_DownloadButton = new QPushButton{ "Download" };
        m_CancelButton = new QPushButton{ "Cancel" };

        auto* layout{ new QHBoxLayout };
        layout->addWidget(m_DownloadButton);
        layout->addWidget(m_CancelButton);
        m_Buttons->setLayout(layout);

        QObject::connect(m_DownloadButton,
                         &QPushButton::clicked,
                         this,
                         &CardDownloaderPopup::StartDownload);
        QObject::connect(m_CancelButton,
                         &QPushButton::clicked,
                         this,
                         &QDialog::close);
    }

    const auto text_changed{
        [this]()
        {
            const QString& text{ m_TextInput->toPlainText() };
            TextChanged(text);
        }
    };

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

    QObject::connect(m_TextInput,
                     &QTextEdit::textChanged,
                     this,
                     text_changed);

    QObject::connect(m_UpscaleModel,
                     &QComboBox::currentTextChanged,
                     this,
                     change_upscale_model);

    m_OutputDir.setAutoRemove(true);

    m_Router.PauseCropper();
}

CardDownloaderPopup::~CardDownloaderPopup()
{
    m_Router.UnpauseCropper();
    UninstallLogHook();
}

void CardDownloaderPopup::DownloadProgress(int progress, int target)
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

void CardDownloaderPopup::ImageAvailable(const QByteArray& image_data, const QString& file_name)
{
    LogInfo("Received data for card {}", file_name.toStdString());

    const bool fill_corners{ FillCorners() };
    auto upscale_model{ UpscaleModel() };

    std::vector<QString> out_files{ m_Downloader->GetDuplicates(file_name) };
    out_files.insert(out_files.begin(), file_name);
    for (auto& out_file : out_files)
    {
        out_file = m_OutputDir.filePath(out_file);
    }

    auto* worker{ new CardDownloaderImageWorker{
        m_Project,
        file_name,
        image_data,
        fill_corners,
        std::move(upscale_model),
        m_Project.CardSize(),
        g_Cfg.m_MaxDPI,
        std::move(out_files),
    } };
    QObject::connect(worker,
                     &CardDownloaderImageWorker::Done,
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

void CardDownloaderPopup::StartDownload()
{
    ValidateSettings();

    PreDownload();

    m_NetworkManager = std::make_unique<QNetworkAccessManager>(this);
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

    InstallLogHook();

    const auto upscale_model{ UpscaleModel().toStdString() };
    auto do_download{
        [this, upscale_model]()
        {
            if (!upscale_model.empty())
            {
                LogInfo("Preloading upscaling model...");
                auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
                LoadModel(*app, upscale_model);
            }

            const bool clear_image_folder{ ClearImageFolder() };
            auto skip_images{
                (!clear_image_folder
                     ? ListImageFiles(m_Project.m_Data.m_ImageDir, m_Project.m_Data.m_CropDir)
                     : std::vector<fs::path>{}) |
                std::views::transform(static_cast<QString (*)(const fs::path&)>(&ToQString)) |
                std::ranges::to<std::vector>()
            };

            auto backside_pattern{
                DownloadBacksides()
                    ? std::optional{ ToQString(m_Project.m_Data.m_BacksideAutoPattern) }
                    : std::nullopt
            };

            m_Downloader = MakeDownloader(std::move(skip_images), std::move(backside_pattern));

            if (m_Downloader->ParseInput(m_TextInput->toPlainText()))
            {
                connect(m_NetworkManager.get(),
                        &QNetworkAccessManager::finished,
                        m_Downloader.get(),
                        &CardArtDownloader::HandleReply);

                connect(m_Downloader.get(),
                        &CardArtDownloader::Progress,
                        this,
                        &CardDownloaderPopup::DownloadProgress);
                connect(m_Downloader.get(),
                        &CardArtDownloader::ImageAvailable,
                        this,
                        &CardDownloaderPopup::ImageAvailable);

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

            OnDownload();
            m_DownloadButton->setDisabled(true);
            m_UpscaleModel->setDisabled(true);
        }
    };

    if (!upscale_model.empty())
    {
        if (ModelRequiresDownload(upscale_model))
        {
            if (upscale_model != "None")
            {
                if (const auto url{ GetModelUrl(upscale_model) })
                {
                    LogInfo("Downloading model {}", upscale_model);
                    m_ProgressBar->setVisible(true);

                    QNetworkRequest get_request{ ToQString(url.value()) };
                    QNetworkReply* reply{ m_NetworkManager->get(std::move(get_request)) };

                    QObject::connect(reply,
                                     &QNetworkReply::finished,
                                     this,
                                     [this, upscale_model, reply, do_download]()
                                     {
                                         {
                                             QFile file(ToQString(GetModelFilename(upscale_model)));
                                             file.open(QIODevice::WriteOnly);
                                             file.write(reply->readAll());
                                         }

                                         m_ProgressBar->setVisible(false);
                                         reply->deleteLater();

                                         // Initiate the actual download
                                         do_download();
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

                    // Quit here, we initiate once we are done downloading
                    return;
                }
            }
        }
    }

    do_download();
}

void CardDownloaderPopup::FinalizeDownload()
{
    const auto upscale_model{ UpscaleModel().toStdString() };
    if (!upscale_model.empty())
    {
        auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
        UnloadModel(*app, upscale_model);
    }

    const auto downloaded_files{ m_Downloader->GetFiles() };
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

    const bool clear_image_folder{ ClearImageFolder() };
    if (clear_image_folder)
    {
        std::vector<fs::path> cards_to_remove;
        for (const auto& card_info : m_Project.m_Data.m_Cards)
        {
            if (!DownloadBacksides() &&
                card_info.m_Name == m_Project.m_Data.m_BacksideDefault)
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
        if (DownloadBacksides())
        {
            if (const std::optional backside{ m_Downloader->GetBackside(card) })
            {
                m_Project.SetBacksideImage(path, backside.value().toStdString());
            }
        }
    }

    if (DownloadBacksides())
    {
        m_Project.m_Data.m_BacksideDefault = "__back.png";
    }

    m_Router.RefreshCardGrid();
    m_Router.UnpauseCropper();

    m_CancelButton->setText("Close");

    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
    main_window->Toast(ToastType::Info,
                       "Download done",
                       "All files have been downloaded, you can now close the plugin.");
}

void CardDownloaderPopup::InstallLogHook()
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

void CardDownloaderPopup::UninstallLogHook()
{
    if (m_LogHookId.has_value())
    {
        Log::GetInstance(Log::c_MainLogName)->UninstallHook(m_LogHookId.value());
    }
}

QString CardDownloaderPopup::OutputDir() const
{
    return m_OutputDir.path();
}
