#include <ppp/ui/options/widget_project_options.hpp>

#include <ranges>

#include <QAction>
#include <QCursor>
#include <QGridLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups/new_project_popup.hpp>

#include <ppp/ui/view_models/options/view_model_project_options.hpp>
#include <ppp/ui/view_models/util.hpp>

#include <ppp/profile/profile.hpp>

ProjectOptionsWidget::ProjectOptionsWidget(ProjectOptionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Project");
    m_ViewModel.setParent(this);

    m_ProjectName = new QLineEdit{ "Project Name" };
    auto* new_button{ new QPushButton{ "New Project" } };
    auto* save_button{ new QPushButton{ "Save Project" } };
    auto* load_button{ new QPushButton{ "Load Project" } };

    auto* layout{ new QGridLayout };
    layout->addWidget(m_ProjectName, 0, 0, 1, 2);
    layout->addWidget(new_button, 1, 0, 1, 2);
    layout->addWidget(save_button, 2, 0);
    layout->addWidget(load_button, 2, 1);
    setLayout(layout);

    QObject::connect(m_ProjectName,
                     &QLineEdit::textChanged,
                     &m_ViewModel,
                     &ProjectOptionsViewModel::ChangeProjectName);
    QObject::connect(new_button,
                     &QPushButton::clicked,
                     this,
                     &ProjectOptionsWidget::NewProjectClicked);
    QObject::connect(save_button,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     &ProjectOptionsViewModel::SaveProject);
    QObject::connect(load_button,
                     &QPushButton::clicked,
                     this,
                     &ProjectOptionsWidget::LoadProjectClicked);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(ProjectPathChanged);

    m_ViewModel.EmitDefaults();
}

void ProjectOptionsWidget::ProjectPathChanged(const fs::path& project_path)
{
    m_ProjectName->blockSignals(true);
    m_ProjectName->setText(ToQString(project_path.stem()));
    m_ProjectName->blockSignals(false);
}

void ProjectOptionsWidget::NewProjectClicked()
{
    TRACY_AUTO_SCOPE();

    auto* main_window{ window() };
    main_window->setEnabled(false);
    AtScopeExit reenable_window{
        [=]()
        { main_window->setEnabled(true); }
    };

    auto* new_project_view_model{ m_ViewModel.MakeProjectPopupViewModel() };
    NewProjectPopup new_project_wizard{ nullptr, new_project_view_model };
    new_project_wizard.Show();

    if (m_ViewModel.VerifyNewProjectOptions(*new_project_view_model))
    {
        m_ViewModel.CreateNewProject(*new_project_view_model);
    }
}
void ProjectOptionsWidget::LoadProjectClicked()
{
    TRACY_AUTO_SCOPE();

    const auto projects_root{ m_ViewModel.GetProjectsRoot() };

    auto* project_menu{ new QMenu{ this } };
    ForEachFile(
        projects_root,
        [this, projects_root, project_menu](const fs::path& file_name)
        {
            if (m_ViewModel.IsCurrentProject(file_name))
            {
                return;
            }

            auto* action{ project_menu->addAction(ToQString(file_name.stem())) };
            connect(
                action,
                &QAction::triggered,
                this,
                [this, projects_root, file_name]()
                {
                    if (m_ViewModel.VerifyLoadProject())
                    {
                        m_ViewModel.LoadProject(projects_root / file_name);
                    }
                });
        },
        std::array{ ".json"_p });

    project_menu->addSeparator();

    auto* action{ project_menu->addAction("Change Projects Locaation") };
    connect(
        action,
        &QAction::triggered,
        this,
        [this, projects_root]()
        {
            TRACY_AUTO_SCOPE();
            if (const auto new_projects_root{ OpenFolderDialog(projects_root) })
            {
                m_ViewModel.ChangeProjectsRoot(ToQString(new_projects_root.value()));
            }
        });

    project_menu->popup(QCursor::pos());
}
