#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader_popup.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

#include <ppp/project/project.hpp>

#include <ppp/plugins/decklist_textbox.hpp>
#include <ppp/plugins/plugin_interface.hpp>
#include <ppp/plugins/yugioh_card_downloader/download_ygoprodeck.hpp>

YuGiOhDownloaderPopup::YuGiOhDownloaderPopup(QWidget* parent,
                                             Project& project,
                                             PluginInterface& router)
    : CardDownloaderPopup{ parent, project, router }
{
    m_AutoCenter = false;
    setWindowFlags(Qt::WindowType::Dialog);

    m_Settings = new QCheckBox{ "Adjust Settings" };
    m_Settings->setChecked(true);

    m_ClearCheckbox = new QCheckBox{ "Clear Image Folder" };
    m_ClearCheckbox->setChecked(true);

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_TextInput);
    layout->addWidget(m_Settings);
    layout->addWidget(m_ClearCheckbox);
    layout->addWidget(m_Upscale);
    layout->addWidget(m_Hint);
    layout->addWidget(m_ProgressBar);
    layout->addWidget(m_Buttons);
    setLayout(layout);
}

bool YuGiOhDownloaderPopup::ClearImageFolder() const
{
    return m_ClearCheckbox->isChecked();
}
bool YuGiOhDownloaderPopup::DownloadBacksides() const
{
    return false;
}
bool YuGiOhDownloaderPopup::FillCorners() const
{
    return false;
}
QString YuGiOhDownloaderPopup::UpscaleModel() const
{
    const auto model{ m_UpscaleModel->currentText() };
    if (model == "None")
    {
        return "";
    }
    return model;
}

void YuGiOhDownloaderPopup::TextChanged(const QString& /*text*/)
{
}
void YuGiOhDownloaderPopup::PreDownload()
{
}
std::unique_ptr<CardArtDownloader> YuGiOhDownloaderPopup::MakeDownloader(
    std::vector<QString> skip_files,
    std::optional<QString> /*backside_pattern*/)
{
    LogInfo("Downloading YuGiOh Decklist to {}",
            OutputDir().toStdString());
    return std::make_unique<YGOProDeckDownloader>(
        std::move(skip_files));
}
void YuGiOhDownloaderPopup::OnDownload()
{
    m_Settings->setDisabled(true);
    m_ClearCheckbox->setDisabled(true);
}
void YuGiOhDownloaderPopup::PostDownload()
{
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
