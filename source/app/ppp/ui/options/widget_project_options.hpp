#pragma once

#include <QWidget>

#include <ppp/util.hpp>

class QPushButton;
class QProgressBar;

class Project;
class Config;
struct ProjectData;

class ProjectOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    ProjectOptionsWidget(Project& project,
                         const Config& config);

  signals:
    void NewProjectOpened(const ProjectData& old_project, const ProjectData& new_project);
};
