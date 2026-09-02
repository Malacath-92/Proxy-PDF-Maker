#include <ppp/ui/view_models/options/view_model_project_options.hpp>

#include <QMessageBox>

#include <ranges>

#include <ppp/app.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/view_models/options/view_model_new_project_popup.hpp>

#include <ppp/profile/profile.hpp>

ProjectOptionsViewModel::ProjectOptionsViewModel(Project& project,
                                                 const Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
    TRACY_AUTO_SCOPE();
}

void ProjectOptionsViewModel::ChangeProjectName(const QString& project_name)
{
    auto* application{ ppApp };
    application->SetProjectName(project_name.toStdString());
}
void ProjectOptionsViewModel::ChangeProjectsRoot(const QString& projects_root)
{
    auto* application{ ppApp };
    application->SetProjectsRoot(projects_root.toStdString());
}
void ProjectOptionsViewModel::CreateNewProject(const NewProjectPopupViewModel& view_model)
{
    TRACY_AUTO_SCOPE();

    if (!view_model.CreateNewProject())
    {
        return;
    }

    const fs::path new_image_folder{
        view_model.NewImageFolder()
    };

    if (view_model.ClearImages())
    {
        const auto num_images{ CountImageFiles(new_image_folder) };
        if (num_images > 0)
        {
            const auto response{
                QMessageBox::question(
                    nullptr,
                    "Clear Images",
                    QString{ "This will delete %1 image files. Are you sure you want to continue?" }
                        .arg(num_images))
            };
            if (response == QMessageBox::StandardButton::No)
            {
                return;
            }
        }
    }

    auto& application{ *ppApp };
    application.SetProjectPath(QString{ "%1.json" }
                                   .arg(view_model.NewProjectName())
                                   .toStdString());

    for (const auto& entry : std::filesystem::directory_iterator(new_image_folder))
    {
        std::filesystem::remove_all(entry.path());
    }

    Project new_project{ m_Cfg, application.GetProjectsFolder(), application.GetBasePdfsFolder() };
    new_project.m_Data.m_FileName = view_model.NewProjectName().toStdString();
    new_project.m_Data.m_ImageDir = new_image_folder;
    new_project.m_Data.m_CropDir = new_project.m_Data.m_ImageDir / "crop";
    new_project.m_Data.m_UncropDir = new_project.m_Data.m_ImageDir / "uncrop";
    new_project.m_Data.m_ImageCache = new_project.m_Data.m_CropDir / "preview.cache";
    new_project.m_Data.m_CardSizeChoice = view_model.NewCardSize().toStdString();
    new_project.m_Data.m_PageSize = view_model.NewPaperSize().toStdString();

    m_Project.LoadFromJson(new_project.DumpToJson(), &application);
}
void ProjectOptionsViewModel::SaveProject()
{
    TRACY_AUTO_SCOPE();

    auto& application{ *ppApp };
    m_Project.Dump(application.GetProjectPath());
}
void ProjectOptionsViewModel::LoadProject(const fs::path& project_path)
{
    TRACY_AUTO_SCOPE();

    auto& application{ *ppApp };
    if (project_path != application.GetProjectPath())
    {
        application.SetProjectPath(project_path);

        m_Project.Load(project_path);
    }
}

void ProjectOptionsViewModel::EmitDefaults()
{
    const auto* application{ ppApp };
    ProjectPathChanged(application->GetProjectPath());
}

fs::path ProjectOptionsViewModel::GetProjectsRoot() const
{
    const auto* application{ ppApp };
    return application->GetProjectPath().parent_path();
}

NewProjectPopupViewModel* ProjectOptionsViewModel::MakeProjectPopupViewModel() const
{
    return new NewProjectPopupViewModel{ m_Cfg };
}
