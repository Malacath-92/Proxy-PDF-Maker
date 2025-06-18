#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <fmt/ranges.h>

#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

#include <ppp/image.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/widget_label.hpp>

#include <ppp/plugins/mtg_card_downloader/download_mpcfill.hpp>

MtgDownloaderPopup::MtgDownloaderPopup(QWidget* parent)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);

    m_TextInput = new QTextEdit;
    m_TextInput->setLineWrapMode(QTextEdit::LineWrapMode::FixedColumnWidth);
    m_TextInput->setPlaceholderText("Paste decklist or MPC Autofill xml");

    m_Hint = new QLabel;
    m_Hint->setVisible(false);

    m_ProgressBar = new QProgressBar;
    m_ProgressBar->setTextVisible(true);
    m_ProgressBar->setVisible(false);

    auto* buttons{ new QWidget{} };
    {
        m_DownloadButton = new QPushButton{ "Download" };
        auto* cancel_button{ new QPushButton{ "Cancel" } };

        auto* layout{ new QHBoxLayout };
        layout->addWidget(m_DownloadButton);
        layout->addWidget(cancel_button);
        buttons->setLayout(layout);

        QObject::connect(m_DownloadButton,
                         &QPushButton::clicked,
                         this,
                         &MtgDownloaderPopup::DoDownload);
        QObject::connect(cancel_button,
                         &QPushButton::clicked,
                         this,
                         &QDialog::close);
    }

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_TextInput);
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
            switch (m_InputType)
            {
            case InputType::Decklist:
                m_Hint->setText("Input inferred as Moxfield or Archidekt list...");
                break;
            case InputType::MPCAutofill:
                m_Hint->setText("Input inferred as MPC Autofill xml...");
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
}

MtgDownloaderPopup::~MtgDownloaderPopup()
{
    UninstallLogHook();
}

void MtgDownloaderPopup::MPCFillRequestFinished(QNetworkReply* reply)
{
    if (auto* set{ std::any_cast<MPCFillSet>(&m_CardsData) })
    {
        const auto file_name{
            [&]()
            {
                const QString id{ MPCFillIdFromUrl(reply->request().url().toString()) };
                for (const auto& card : set->m_Frontsides)
                {
                    if (card.m_Id == id)
                    {
                        return card.m_Name;
                    }

                    if (card.m_Backside.has_value() && card.m_Backside.value().m_Id == id)
                    {
                        return card.m_Backside.value().m_Name;
                    }
                }

                return QString{ "__back.png" };
            }()
        };

        LogInfo("Received data for card {}", file_name.toStdString());
        const auto downloaded_data{ ImageDataFromReply(reply->readAll()) };

        {
            QFile file(m_OutputDir.filePath(file_name));
            file.open(QIODevice::WriteOnly);
            file.write(downloaded_data);
        }

        {
            // Handle same frontsides with different backsides
            auto duplicates{
                set->m_Frontsides |
                    std::views::filter([&file_name](const auto& card)
                                       { return card.m_Name == file_name; }) |
                    std::views::drop(1),
            };
#if __cpp_lib_ranges_enumerate
            for (auto [i, card] : duplicates | std::views::enumerate)
            {
#else
            size_t i{ 0 };
            for (auto& card : duplicates)
            {
#endif
                card.m_Name = card.m_Name.replace(QRegularExpression{ "(\\.\\w+)" }, QString{ " - Copy %1\\1" }.arg(i));

                QFile file(m_OutputDir.filePath(card.m_Name));
                file.open(QIODevice::WriteOnly);
                file.write(downloaded_data);
#if not __cpp_lib_ranges_enumerate
                ++i;
#endif
            }
        }

        ++m_NumReplies;
        m_ProgressBar->setValue(static_cast<int>(m_NumReplies));
    }

    reply->deleteLater();
}

void MtgDownloaderPopup::DoDownload()
{
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

    switch (m_InputType)
    {
    default:
    case InputType::Decklist:
        break;
    case InputType::MPCAutofill:
        LogInfo("Downloading MPCFill files to {}", m_OutputDir.path().toStdString());
        if (std::optional set{ ParseMPCFill(m_TextInput->toPlainText()) })
        {
            m_CardsData = std::move(set).value();
            connect(m_NetworkManager.get(),
                    &QNetworkAccessManager::finished,
                    this,
                    &MtgDownloaderPopup::MPCFillRequestFinished);
            m_NumRequests = BeginDownloadMPCFill(*m_NetworkManager, std::any_cast<MPCFillSet>(m_CardsData));

            m_ProgressBar->setMaximum(static_cast<int>(m_NumRequests));
            m_ProgressBar->setValue(0);
            m_ProgressBar->setVisible(true);
        }
        break;
    }

    m_DownloadButton->setDisabled(true);
    m_DownloadButton->setEnabled(false);
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

InputType MtgDownloaderPopup::StupidInferSource(const QString& text)
{
    if (text.startsWith("<order>") && text.endsWith("</order>"))
    {
        return InputType::MPCAutofill;
    }

    const auto is_decklist{
        [&]()
        {
            const auto lines{
                text.split(QRegularExpression{ "[\r\n]" }, Qt::SplitBehaviorFlags::SkipEmptyParts)
            };
            for (const auto& line : lines)
            {
                if (line.endsWith(":"))
                {
                    continue;
                }

                if (!g_DecklistRegex.match(line).hasMatch())
                {
                    return false;
                }
            }
            return true;
        }()
    };
    if (is_decklist)
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
