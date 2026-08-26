#pragma once

#include <QWidget>

class ProjectOptionsViewModel;

class ProjectOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    ProjectOptionsWidget(ProjectOptionsViewModel* view_model);

  private:
    ProjectOptionsViewModel& m_ViewModel;
};
