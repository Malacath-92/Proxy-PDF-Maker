#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <fmt/ranges.h>

#include <QCheckBox>
#include <QDropEvent>
#include <QLabel>
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

#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/widget_label.hpp>

#include <ppp/plugins/mtg_card_downloader/decklist_parser.hpp>
#include <ppp/plugins/mtg_card_downloader/download_mpcfill.hpp>
#include <ppp/plugins/mtg_card_downloader/download_scryfall.hpp>
#include <ppp/plugins/plugin_interface.hpp>

class DownloaderTextEdit : public QTextEdit
{
    virtual void insertFromMimeData(const QMimeData* source) override
    {
        if (source->hasText())
        {
            const auto url{ QUrl::fromUserInput(source->text()) };
            if (url.isValid() && url.isLocalFile() && QFile::exists(url.toLocalFile()))
            {
                QFile file{ url.toLocalFile() };
                if (file.open(QFile::OpenModeFlag::ReadOnly))
                {
                    insertPlainText(file.readAll());
                    return;
                }
            }
        }

        QTextEdit::insertFromMimeData(source);
    }
};

MtgDownloaderImageWorker::MtgDownloaderImageWorker(const Project& project,
                                                   const QByteArray& image_data,
                                                   bool fill_corners,
                                                   std::vector<QString> out_files)
    : m_Project{ project }
    , m_ImageData{ image_data }
    , m_FillCorners{ fill_corners }
    , m_OutFiles{ std::move(out_files) }
{
}

void MtgDownloaderImageWorker::run()
{
    if (m_FillCorners)
    {
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

    for (const auto& out_file : m_OutFiles)
    {
        QFile file(out_file);
        file.open(QIODevice::WriteOnly);
        file.write(m_ImageData);
    }

    Done();
}

MtgDownloaderPopup::MtgDownloaderPopup(QWidget* parent,
                                       Project& project,
                                       PluginInterface& router)
    : PopupBase{ parent }
    , m_Project{ project }
    , m_Router{ router }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);

    m_TextInput = new DownloaderTextEdit;
    m_TextInput->setPlaceholderText("Paste decklist (Moxfield, Archidekt, MODO, or MTGA), MPC Autofill xml, or a Scryfall query prepended with $");

    m_Settings = new QCheckBox{ "Adjust Settings" };
    m_Settings->setChecked(true);

    m_Backsides = new QCheckBox{ "Download Backsides" };
    m_Backsides->setChecked(true);

    m_ClearCheckbox = new QCheckBox{ "Clear Image Folder" };
    m_ClearCheckbox->setChecked(true);

    m_FillCornersCheckbox = new QCheckBox{ "Fill Corners" };
    m_FillCornersCheckbox->setChecked(true);

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
                         &MtgDownloaderPopup::DoDownload);
        QObject::connect(m_CancelButton,
                         &QPushButton::clicked,
                         this,
                         &QDialog::close);
    }

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_TextInput);
    layout->addWidget(m_Settings);
    layout->addWidget(m_Backsides);
    layout->addWidget(m_ClearCheckbox);
    layout->addWidget(m_FillCornersCheckbox);
    layout->addWidget(m_Hint);
    layout->addWidget(m_ProgressBar);
    layout->addWidget(buttons);
    setLayout(layout);

    const auto text_changed{
        [this]()
        {
            const QString& text{ m_TextInput->toPlainText() };
            m_InputType = StupidInferSource(text);

            m_Hint->setVisible(true);
            m_FillCornersCheckbox->setEnabled(true);

            switch (m_InputType)
            {
            case InputType::Decklist:
                m_Hint->setText("Input inferred as decklist...");
                break;
            case InputType::ScryfallQuery:
                m_Hint->setText("Input inferred as Scryfall query...");
                break;
            case InputType::MPCAutofill:
                m_Hint->setText("Input inferred as MPC Autofill xml...");
                m_FillCornersCheckbox->setEnabled(false);
                break;
            case InputType::None:
            default:
                m_Hint->setText("Can't infer input type...");
                break;
            }
        }
    };

    QObject::connect(m_TextInput,
                     &QTextEdit::textChanged,
                     this,
                     text_changed);

    m_OutputDir.setAutoRemove(true);

    m_Router.PauseCropper();
}

MtgDownloaderPopup::~MtgDownloaderPopup()
{
    m_Router.UnpauseCropper();
    UninstallLogHook();
}

void MtgDownloaderPopup::DownloadProgress(int progress, int target)
{
    m_ProgressBar->setValue(progress);
    m_ProgressBar->setMaximum(target);

    if (progress == target)
    {
        m_DownloaderDone = true;
    }
}

void MtgDownloaderPopup::ImageAvailable(const QByteArray& image_data, const QString& file_name)
{
    LogInfo("Received data for card {}", file_name.toStdString());

    const bool fill_corners{ m_FillCornersCheckbox->isChecked() && !m_Downloader->ProvidesBleedEdge() };

    std::vector<QString> out_files{ m_Downloader->GetDuplicates(file_name) };
    out_files.insert(out_files.begin(), file_name);
    for (auto& out_file : out_files)
    {
        out_file = m_OutputDir.filePath(out_file);
    }

    auto* worker{ new MtgDownloaderImageWorker{ m_Project, image_data, fill_corners, std::move(out_files) } };
    QObject::connect(worker,
                     &MtgDownloaderImageWorker::Done,
                     this,
                     [this]()
                     {
                         m_WaitingForImages--;
                         if (m_WaitingForImages == 0 && m_DownloaderDone)
                         {
                             FinalizeDownload();
                         }
                     });

    m_WaitingForImages++;
    QThreadPool::globalInstance()->start(worker);
}

void MtgDownloaderPopup::DoDownload()
{
    ValidateSettings();

    if (m_InputType == InputType::None)
    {
        SelectInputTypePopup type_selector{ nullptr };

        setEnabled(false);
        type_selector.Show();
        setEnabled(true);

        m_InputType = type_selector.GetInputType();

        DoDownload();
        return;
    }

    InstallLogHook();

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

    const bool clear_image_folder{ m_ClearCheckbox->isChecked() };
    const auto skip_images{
        (!clear_image_folder
             ? ListImageFiles(m_Project.m_Data.m_ImageDir, m_Project.m_Data.m_CropDir)
             : std::vector<fs::path>{}) |
        std::views::transform(static_cast<QString (*)(const fs::path&)>(&ToQString)) |
        std::ranges::to<std::vector>()
    };

    const auto backside_pattern{
        m_Backsides->isChecked()
            ? std::optional{ ToQString(m_Project.m_Data.m_BacksideAutoPattern) }
            : std::nullopt
    };

    switch (m_InputType)
    {
    default:
    case InputType::Decklist:
        [[fallthrough]];
    case InputType::ScryfallQuery:
        LogInfo("Downloading {} to {}",
                m_InputType == InputType::Decklist
                    ? "Decklist"
                    : "Scryfall query",
                m_OutputDir.path().toStdString());
        m_Downloader = std::make_unique<ScryfallDownloader>(
            std::move(skip_images),
            backside_pattern);
        break;
    case InputType::MPCAutofill:
        LogInfo("Downloading MPCFill files to {}", m_OutputDir.path().toStdString());
        m_Downloader = std::make_unique<MPCFillDownloader>(
            std::move(skip_images),
            backside_pattern);
        break;
    }

    if (m_Downloader->ParseInput(m_TextInput->toPlainText()))
    {
        connect(m_NetworkManager.get(),
                &QNetworkAccessManager::finished,
                m_Downloader.get(),
                &CardArtDownloader::HandleReply);

        connect(m_Downloader.get(),
                &CardArtDownloader::Progress,
                this,
                &MtgDownloaderPopup::DownloadProgress);
        connect(m_Downloader.get(),
                &CardArtDownloader::ImageAvailable,
                this,
                &MtgDownloaderPopup::ImageAvailable);

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
    m_Backsides->setDisabled(true);
    m_ClearCheckbox->setDisabled(true);
    m_FillCornersCheckbox->setDisabled(true);
}

void MtgDownloaderPopup::FinalizeDownload()
{
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
        const auto images{
            ListImageFiles(target_dir)
        };
        for (const auto& img : images)
        {
            if (!std::ranges::contains(downloaded_file_paths, img) &&
                fs::exists(target_dir / img))
            {
                fs::remove(target_dir / img);
            }
            m_Project.CardRemoved(img);
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

        if (const std::optional backside{ m_Downloader->GetBackside(card) })
        {
            m_Project.SetBacksideImage(path, backside.value().toStdString());
        }
        else
        {
            m_Project.SetBacksideImageDefault(path);
        }
    }
    m_Project.m_Data.m_BacksideDefault = "__back.png";

    m_Router.RefreshCardGrid();
    m_Router.UnpauseCropper();

    m_CancelButton->setText("Close");

    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
    main_window->Toast(ToastType::Info,
                       "MtG Download done",
                       "All files have been downloaded, you can now close the plugin.");
}

void MtgDownloaderPopup::InstallLogHook()
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

void MtgDownloaderPopup::UninstallLogHook()
{
    if (m_LogHookId.has_value())
    {
        Log::GetInstance(Log::c_MainLogName)->UninstallHook(m_LogHookId.value());
    }
}

void MtgDownloaderPopup::ValidateSettings()
{
    if (!m_Settings->isChecked())
    {
        return;
    }

    if (m_Project.m_Data.m_CardSizeChoice != "Standard" && m_Project.m_Data.m_CardSizeChoice != "Standard x2")
    {
        m_Router.SetCardSizeChoice("Standard");
    }

    if (m_Backsides->isChecked() != m_Project.m_Data.m_BacksideEnabled)
    {
        m_Router.SetEnableBackside(m_Backsides->isChecked());
    }

    if (m_Project.m_Data.m_BacksideAutoPattern.empty())
    {
        m_Router.SetBacksideAutoPattern("__back_$");
    }
}

InputType MtgDownloaderPopup::StupidInferSource(const QString& text)
{
    if (text.startsWith("$"))
    {
        return InputType::ScryfallQuery;
    }

    if (text.startsWith("<order>") && text.endsWith("</order>"))
    {
        return InputType::MPCAutofill;
    }

    if (InferDecklistType(text) != DecklistType::None)
    {
        return InputType::Decklist;
    }

    return InputType::None;
}

SelectInputTypePopup::SelectInputTypePopup(QWidget* parent)
    : PopupBase{ parent }
{
    auto* instructions{ new QLabel{ "Choose a valid input type to force..." } };

    auto* input_type{ new ComboBoxWithLabel{
        "Input Type", magic_enum::enum_names<InputType>(), magic_enum::enum_name(InputType::None) } };

    auto* ok_button{ new QPushButton{ "Ok" } };
    ok_button->setDisabled(true);

    auto* buttons_layout{ new QHBoxLayout };
    buttons_layout->addWidget(ok_button);

    auto* buttons{ new QWidget{} };
    buttons->setLayout(buttons_layout);

    auto* layout{ new QVBoxLayout };
    layout->addWidget(instructions);
    layout->addWidget(input_type);
    layout->addWidget(buttons);
    setLayout(layout);

    auto change_input_type{
        [this, ok_button](const QString& t)
        {
            m_InputType = magic_enum::enum_cast<InputType>(t.toStdString())
                              .value_or(InputType::None);

            ok_button->setEnabled(m_InputType != InputType::None);
        }
    };

    QObject::connect(ok_button,
                     &QPushButton::clicked,
                     this,
                     &QDialog::close);
    QObject::connect(input_type->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_input_type);
}
