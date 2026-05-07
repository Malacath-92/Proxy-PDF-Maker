#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_popup.hpp>

#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_label.hpp>

#include <ppp/plugins/decklist_textbox.hpp>
#include <ppp/plugins/mtg_card_downloader/decklist_parser.hpp>
#include <ppp/plugins/mtg_card_downloader/download_mpcfill.hpp>
#include <ppp/plugins/mtg_card_downloader/download_scryfall.hpp>
#include <ppp/plugins/plugin_interface.hpp>

MtgDownloaderPopup::MtgDownloaderPopup(QWidget* parent,
                                       Project& project,
                                       PluginInterface& router)
    : CardDownloaderPopup{ parent, project, router }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);

    m_Settings = new QCheckBox{ "Adjust Settings" };
    m_Settings->setChecked(true);

    m_Backsides = new QCheckBox{ "Download Backsides" };
    m_Backsides->setChecked(true);
    m_Backsides->setToolTip("If unticked, will maintain current default backside if any.");

    m_ClearCheckbox = new QCheckBox{ "Clear Image Folder" };
    m_ClearCheckbox->setChecked(true);

    m_FillCornersCheckbox = new QCheckBox{ "Fill Corners" };
    m_FillCornersCheckbox->setChecked(true);

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_TextInput);
    layout->addWidget(m_Settings);
    layout->addWidget(m_Backsides);
    layout->addWidget(m_ClearCheckbox);
    layout->addWidget(m_FillCornersCheckbox);
    layout->addWidget(m_Upscale);
    layout->addWidget(m_Hint);
    layout->addWidget(m_ProgressBar);
    layout->addWidget(m_Buttons);
    setLayout(layout);
}

bool MtgDownloaderPopup::ClearImageFolder() const
{
    return m_ClearCheckbox->isChecked();
}
bool MtgDownloaderPopup::DownloadBacksides() const
{
    return m_Backsides->isChecked();
}
bool MtgDownloaderPopup::FillCorners() const
{
    return m_FillCornersCheckbox->isChecked();
}
QString MtgDownloaderPopup::UpscaleModel() const
{
    if (m_InputType == InputType::Decklist)
    {
        const auto model{ m_UpscaleModel->currentText() };
        if (model == "None")
        {
            return "";
        }
        return model;
    }
    return "";
}

void MtgDownloaderPopup::TextChanged(const QString& text)
{
    m_InputType = StupidInferSource(text);

    m_Hint->setVisible(true);
    m_FillCornersCheckbox->setEnabled(true);
    m_UpscaleModel->setEnabled(true);

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
        m_UpscaleModel->setEnabled(false);
        break;
    case InputType::None:
    default:
        m_Hint->setText("Can't infer input type...");
        break;
    }
}
std::unique_ptr<CardArtDownloader> MtgDownloaderPopup::MakeDownloader(
    std::vector<QString> skip_files,
    std::optional<QString> backside_pattern)
{
    switch (m_InputType)
    {
    default:
    case InputType::Decklist:
        [[fallthrough]];
    case InputType::ScryfallQuery:
        LogInfo("Downloading {} to {}",
                m_InputType == InputType::Decklist
                    ? "MtG Decklist"
                    : "Scryfall query",
                OutputDir().toStdString());
        return std::make_unique<ScryfallDownloader>(
            std::move(skip_files),
            backside_pattern);
    case InputType::MPCAutofill:
        LogInfo("Downloading MPCFill files to {}", OutputDir().toStdString());
        return std::make_unique<MPCFillDownloader>(
            std::move(skip_files),
            backside_pattern);
    }
}
void MtgDownloaderPopup::PreDownload()
{
    if (m_InputType == InputType::None)
    {
        SelectInputTypePopup type_selector{ nullptr };

        setEnabled(false);
        type_selector.Show();
        setEnabled(true);

        m_InputType = type_selector.GetInputType();
        ValidateSettings();
    }
}
void MtgDownloaderPopup::OnDownload()
{
    m_Settings->setDisabled(true);
    m_Backsides->setDisabled(true);
    m_ClearCheckbox->setDisabled(true);
    m_FillCornersCheckbox->setDisabled(true);
}
void MtgDownloaderPopup::PostDownload()
{
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

    const bool download_backsides{ m_Backsides->isChecked() };
    if (download_backsides || m_Project.HasValidDefaultBackside())
    {
        m_Router.SetEnableBackside(true);
    }
    else
    {
        m_Router.SetEnableBackside(false);
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
