#include <ppp/ui/options/widget_project_options.hpp>

#include <ranges>

#include <QGridLayout>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>

#include <ppp/app.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups/new_project_popup.hpp>
#include <ppp/ui/popups/popups.hpp>

#include <ppp/profile/profile.hpp>

ProjectOptionsWidget::ProjectOptionsWidget(Project& project,
                                           const Config& config)
{
    TRACY_AUTO_SCOPE();

    setObjectName("Project");

    auto* new_button{ new QPushButton{ "New Project" } };
    auto* save_button{ new QPushButton{ "Save Project" } };
    auto* load_button{ new QPushButton{ "Load Project" } };
    auto* render_alignment_button{ new QPushButton{ "Alignment Test" } };

    const QWidget* buttons[]{
        new_button,
        save_button,
        load_button,
        render_alignment_button,
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
    layout->addWidget(render_alignment_button, 2, 0, 1, 2);
    setLayout(layout);

    const auto new_project{
        [=, this, &project, &config]()
        {
            TRACY_AUTO_SCOPE();

            auto* main_window{ window() };
            main_window->setEnabled(false);
            AtScopeExit reenable_window{
                [=]()
                { main_window->setEnabled(true); }
            };

            NewProjectPopup new_project_wizard{ nullptr, config };
            new_project_wizard.Show();

            if (!new_project_wizard.CreateNewProject())
            {
                return;
            }

            const fs::path new_image_folde{
                new_project_wizard.NewImageFolder().toStdString()
            };
            if (new_project_wizard.ClearImages())
            {
                const auto num_images{ CountImageFiles(new_image_folde) };
                if (num_images > 0)
                {
                    const auto response{
                        QMessageBox::question(
                            main_window,
                            "Clear Images",
                            QString{ "This will delete %1 image files. Are you sure you want to continue?" }
                                .arg(num_images))
                    };
                    if (response == QMessageBox::StandardButton::No)
                    {
                        return;
                    }
                    else
                    {
                        for (const auto& entry : std::filesystem::directory_iterator(new_image_folde))
                        {
                            std::filesystem::remove_all(entry.path());
                        }
                    }
                }
            }

            const auto old_project_data{ std::move(project.m_Data) };
            const auto reset_project_work{
                [&]()
                {
                    TRACY_AUTO_SCOPE();
                    auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
                    application.SetProjectPath(QString{ "%1.json" }
                                                   .arg(new_project_wizard.NewProjectName())
                                                   .toStdString());

                    Project new_project{ config };
                    project.m_Data.m_FileName = new_project_wizard.NewProjectName().toStdString();
                    project.m_Data.m_ImageDir = new_image_folde;
                    project.m_Data.m_CropDir = project.m_Data.m_ImageDir / "crop";
                    project.m_Data.m_UncropDir = project.m_Data.m_ImageDir / "uncrop";
                    project.m_Data.m_ImageCache = project.m_Data.m_CropDir / "preview.cache";
                    project.m_Data.m_CardSizeChoice = new_project_wizard.NewCardSize().toStdString();
                    project.m_Data.m_PageSize = new_project_wizard.NewPaperSize().toStdString();
                    project.LoadFromJson(new_project.DumpToJson(), &application);
                }
            };

            {
                GenericPopup reload_window{ nullptr, "Resetting project..." };
                reload_window.ShowDuringWork(reset_project_work);
            }

            NewProjectOpened(old_project_data, project.m_Data);
        }
    };

    const auto save_project{
        [=, &project]()
        {
            TRACY_AUTO_SCOPE();
            if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Save) })
            {
                auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
                application.SetProjectPath(new_project_json.value());
                project.Dump(new_project_json.value());
            }
        }
    };

    const auto load_project{
        [=, this, &project]()
        {
            TRACY_AUTO_SCOPE();
            if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Open) })
            {
                auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
                if (new_project_json != application.GetProjectPath())
                {
                    application.SetProjectPath(new_project_json.value());
                    GenericPopup reload_window{ nullptr, "Reloading project..." };

                    const auto old_project_data{ std::move(project.m_Data) };
                    const auto load_project_work{
                        [=, &project]()
                        {
                            TRACY_AUTO_SCOPE();
                            project.Load(new_project_json.value());
                        }
                    };

                    auto* main_window{ window() };
                    main_window->setEnabled(false);
                    reload_window.ShowDuringWork(load_project_work);
                    NewProjectOpened(old_project_data, project.m_Data);
                    main_window->setEnabled(true);
                }
            }
        }
    };

    const auto render_alignment{
        [this, &project, &config]()
        {
            TRACY_AUTO_SCOPE();
            auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
            GenericPopup render_align_window{ nullptr, "Rendering alignment PDF..." };

            bool do_error_toast{ false };
            const auto render_work{
                [=, &project, &config, &render_align_window, &do_error_toast]()
                {
                    TRACY_AUTO_SCOPE();
                    const auto uninstall_log_hook{ render_align_window.InstallLogHook() };
                    try
                    {
                        const auto file_path{ GenerateTestPdf(project, config) };
                        OpenFile(file_path);
                    }
                    catch (const std::exception& e)
                    {
                        LogError("Failure while creating pdf: {}\nPlease make sure the file is not opened in another program.", e.what());
                        do_error_toast = !main_window->hasFocus();
                        if (!do_error_toast)
                        {
                            render_align_window.Sleep(3_s);
                        }
                    }
                }
            };

            main_window->setEnabled(false);
            render_align_window.ShowDuringWork(render_work);
            main_window->setEnabled(true);

            if (do_error_toast && !main_window->hasFocus())
            {
                main_window->Toast(ToastType::Error,
                                   "PDF Rendering Error",
                                   "Failure while creating pdf, please check logs for details.");
            }
        }
    };

    QObject::connect(new_button,
                     &QPushButton::clicked,
                     this,
                     new_project);
    QObject::connect(save_button,
                     &QPushButton::clicked,
                     this,
                     save_project);
    QObject::connect(load_button,
                     &QPushButton::clicked,
                     this,
                     load_project);
    QObject::connect(render_alignment_button,
                     &QPushButton::clicked,
                     this,
                     render_alignment);
}
