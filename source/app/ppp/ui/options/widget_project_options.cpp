#include <ppp/ui/options/widget_project_options.hpp>

#include <ranges>

#include <QGridLayout>
#include <QPushButton>

#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups/new_project_popup.hpp>

#include <ppp/ui/view_models/options/view_model_project_options.hpp>

#include <ppp/profile/profile.hpp>

ProjectOptionsWidget::ProjectOptionsWidget(ProjectOptionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Project");
    m_ViewModel.setParent(this);

    auto* new_button{ new QPushButton{ "New Project" } };
    auto* save_button{ new QPushButton{ "Save Project" } };
    auto* load_button{ new QPushButton{ "Load Project" } };

    const QWidget* buttons[]{
        new_button,
        save_button,
        load_button,
    };

    auto widths{ buttons | std::views::transform([](const QWidget* widget)
                                                 { return widget->sizeHint().width(); }) };
    const int32_t minimum_width{ *std::ranges::max_element(widths) };

    auto* layout{ new QGridLayout };
    layout->setColumnMinimumWidth(0, minimum_width + 10);
    layout->setColumnMinimumWidth(1, minimum_width + 10);
    layout->addWidget(new_button, 0, 0, 1, 2);
    layout->addWidget(save_button, 1, 0);
    layout->addWidget(load_button, 1, 1);
    setLayout(layout);

    const auto new_project{
        [this]()
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

            const auto reset_project_work{
                [&]()
                {
                    m_ViewModel.CreateNewProject(*new_project_view_model);
                }
            };

            GenericPopup reload_window{ nullptr, "Resetting project..." };
            reload_window.ShowDuringWork(reset_project_work);
        }
    };

    const auto save_project{
        [this]()
        {
            TRACY_AUTO_SCOPE();
            if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Save) })
            {
                m_ViewModel.SaveProject(new_project_json.value());
            }
        }
    };

    const auto load_project{
        [this]()
        {
            TRACY_AUTO_SCOPE();
            if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Open) })
            {
                m_ViewModel.LoadProject(new_project_json.value());
            }
        }
    };

    QObject::connect(new_button,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     new_project);
    QObject::connect(save_button,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     save_project);
    QObject::connect(load_button,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     load_project);
}
