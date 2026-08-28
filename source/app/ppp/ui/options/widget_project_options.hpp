#pragma once

#include <QWidget>

#include <ppp/util.hpp>

class QLineEdit;

class ProjectOptionsViewModel;

class ProjectOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    ProjectOptionsWidget(ProjectOptionsViewModel* view_model);

  private slots:
    void ProjectPathChanged(const fs::path& project_path);

  private:
    void NewProjectClicked();
    void LoadProjectClicked();

    ProjectOptionsViewModel& m_ViewModel;

    QLineEdit* m_ProjectName;
};
