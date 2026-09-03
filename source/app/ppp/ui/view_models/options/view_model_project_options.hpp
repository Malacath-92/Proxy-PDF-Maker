#pragma once

#include <QObject>

#include <ppp/util.hpp>

class Project;
class Config;

class NewProjectPopupViewModel;

class ProjectOptionsViewModel : public QObject
{
    friend class ProjectOptionsWidget;

    Q_OBJECT

  public:
    ProjectOptionsViewModel(Project& project,
                            const Config& config);

  signals:
    // forward
    void ProjectPathChanged(const fs::path& project_path);

  private slots:
    void ChangeProjectName(const QString& project_name);
    void ChangeProjectsRoot(const QString& projects_root);
    bool VerifyNewProjectOptions(const NewProjectPopupViewModel& view_model) const;
    void CreateNewProject(const NewProjectPopupViewModel& view_model);
    void SaveProject();
    void LoadProject(const fs::path& project_path);

  private:
    void EmitDefaults();

    fs::path GetProjectsRoot() const;

    NewProjectPopupViewModel* MakeProjectPopupViewModel() const;

    Project& m_Project;
    const Config& m_Cfg;
};
