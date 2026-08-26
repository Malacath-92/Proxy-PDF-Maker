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

  private slots:
    void CreateNewProject(const NewProjectPopupViewModel& view_model);
    void SaveProject(const fs::path& project_path);
    void LoadProject(const fs::path& project_path);

  private:
    NewProjectPopupViewModel* MakeProjectPopupViewModel() const;

    Project& m_Project;
    const Config& m_Cfg;
};
