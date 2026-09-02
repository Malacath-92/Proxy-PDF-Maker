#include <ppp/ui/view_models/options/view_model_new_project_popup.hpp>

#include <QApplication>

#include <nlohmann/json.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

NewProjectPopupViewModel::NewProjectPopupViewModel(const Config& config)
    : m_Cfg{ config }
    , m_ProjectName{ "new_project" }
    , m_ImageFolder{ ppApp->GetProjectsFolder() / "images" }
    , m_CardSize{ ToQString(GetDefaultCardSize()) }
    , m_PaperSize{ ToQString(GetDefaultPageSize()) }
{
}

bool NewProjectPopupViewModel::CreateNewProject() const
{
    return !m_Cancelled;
}

QString NewProjectPopupViewModel::NewProjectName() const
{
    return m_ProjectName;
}
fs::path NewProjectPopupViewModel::NewImageFolder() const
{
    return m_ImageFolder;
}
QString NewProjectPopupViewModel::NewCardSize() const
{
    return m_CardSize;
}
QString NewProjectPopupViewModel::NewPaperSize() const
{
    return m_PaperSize;
}
bool NewProjectPopupViewModel::ClearImages() const
{
    return m_ClearImages;
}

void NewProjectPopupViewModel::Cancel()
{
    m_Cancelled = true;
}

void NewProjectPopupViewModel::ChangeProjectName(const QString& project_name)
{
    m_ProjectName = project_name;
}
void NewProjectPopupViewModel::ChangeImageFolder(const fs::path& image_folder)
{
    m_ImageFolder = image_folder;
}
void NewProjectPopupViewModel::ChangeCardSize(const QString& card_size)
{
    m_CardSize = card_size;
}
void NewProjectPopupViewModel::ChangePaperSize(const QString& paper_size)
{
    m_PaperSize = paper_size;
}
void NewProjectPopupViewModel::ChangeClearImages(Qt::CheckState clear_images)
{
    m_ClearImages = clear_images != Qt::CheckState::Unchecked;
}

std::string NewProjectPopupViewModel::GetDefaultCardSize() const
{
    auto* app{ ppApp };
    auto user_default(app->GetProjectDefault("card_size"));
    if (!user_default.is_null())
    {
        return user_default;
    }
    else
    {
        return std::string{ m_Cfg.GetFirstValidCardSize() };
    }
}
const CardSizes& NewProjectPopupViewModel::GetCardSizes() const
{
    return m_Cfg.m_CardSizes;
}

std::string NewProjectPopupViewModel::GetDefaultPageSize() const
{
    auto* app{ ppApp };
    auto user_default(app->GetProjectDefault("page_size"));
    if (!user_default.is_null())
    {
        return user_default;
    }
    else
    {
        return std::string{ m_Cfg.GetFirstValidPageSize() };
    }
}
const PageSizes& NewProjectPopupViewModel::GetPageSizes() const
{
    return m_Cfg.m_PageSizes;
}
