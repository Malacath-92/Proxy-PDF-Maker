#include <ppp/ui/widget_actions.hpp>

#include <ranges>

#include <QGridLayout>
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
#include <ppp/ui/popups.hpp>

#include <ppp/profile/profile.hpp>

ActionsWidget::ActionsWidget(Project& project)
{
    TRACY_AUTO_SCOPE();

    setObjectName("Actions");

    auto* cropper_progress_bar{ new QProgressBar };
    cropper_progress_bar->setToolTip("Cropper Progress");
    cropper_progress_bar->setTextVisible(false);
    cropper_progress_bar->setVisible(false);
    cropper_progress_bar->setRange(0, c_ProgressBarResolution);
    auto* render_button{ new QPushButton{ "Render PDF" } };
    auto* new_button{ new QPushButton{ "New Project" } };
    auto* save_button{ new QPushButton{ "Save Project" } };
    auto* load_button{ new QPushButton{ "Load Project" } };
    auto* set_images_button{ new QPushButton{ "Set Image Folder" } };
    auto* open_images_button{ new QPushButton{ "Open Images" } };
    auto* render_alignment_button{ new QPushButton{ "Alignment Test" } };

    const QWidget* buttons[]{
        cropper_progress_bar,
        render_button,
        new_button,
        save_button,
        load_button,
        set_images_button,
        open_images_button,
        render_alignment_button,
    };

    auto widths{ buttons | std::views::transform([](const QWidget* widget)
                                                 { return widget->sizeHint().width(); }) };
    const int32_t minimum_width{ *std::ranges::max_element(widths) };

    auto* layout{ new QGridLayout };
    layout->setColumnMinimumWidth(0, minimum_width + 10);
    layout->setColumnMinimumWidth(1, minimum_width + 10);
    layout->addWidget(cropper_progress_bar, 0, 0, 1, 2);
    layout->addWidget(render_button, 1, 0, 1, 2);
    layout->addWidget(new_button, 2, 0, 1, 2);
    layout->addWidget(save_button, 3, 0);
    layout->addWidget(load_button, 3, 1);
    layout->addWidget(set_images_button, 4, 0);
    layout->addWidget(open_images_button, 4, 1);
    layout->addWidget(render_alignment_button, 5, 0, 1, 2);
    setLayout(layout);

    const auto render{
        [=, this, &project]()
        {
            auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
            GenericPopup render_window{ main_window, "Rendering PDF..." };

            bool do_error_toast{ false };
            const auto render_work{
                [=, &project, &render_window, &do_error_toast]()
                {
                    const auto uninstall_log_hook{ render_window.InstallLogHook() };

                    try
                    {
                        const auto [frontside_path, backside_path]{ GeneratePdf(project) };
                        OpenFile(frontside_path);
                        if (backside_path.has_value())
                        {
                            OpenFile(backside_path.value());
                        }

                        if (project.m_Data.m_ExportExactGuides)
                        {
                            GenerateCardsSvg(project);
                            GenerateCardsDxf(project);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        LogError("Failure while creating pdf: {}\nPlease make sure the file is not opened in another program.", e.what());
                        do_error_toast = !main_window->hasFocus();
                        if (!do_error_toast)
                        {
                            render_window.Sleep(3_s);
                        }
                    }
                }
            };

            main_window->setEnabled(false);
            render_window.ShowDuringWork(render_work);
            main_window->setEnabled(true);

            if (do_error_toast && !main_window->hasFocus())
            {
                main_window->Toast(ToastType::Error,
                                   "PDF Rendering Error",
                                   "Failure while creating pdf, please check logs for details.");
            }
        }
    };

    const auto new_project{
        [=, this, &project]()
        {
            GenericPopup reload_window{ window(), "Resetting project..." };

            const auto old_project_data{ std::move(project.m_Data) };
            const auto reset_project_work{
                [&project]()
                {
                    auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
                    application.SetProjectPath("proj.json");
                    project.LoadFromJson(Project{}.DumpToJson(), &application);
                }
            };

            auto* main_window{ window() };
            main_window->setEnabled(false);
            reload_window.ShowDuringWork(reset_project_work);
            NewProjectOpened(old_project_data, project.m_Data);
            main_window->setEnabled(true);
        }
    };

    const auto save_project{
        [=, &project]()
        {
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
            if (const auto new_project_json{ OpenProjectDialog(FileDialogType::Open) })
            {
                auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
                if (new_project_json != application.GetProjectPath())
                {
                    application.SetProjectPath(new_project_json.value());
                    GenericPopup reload_window{ window(), "Reloading project..." };

                    const auto old_project_data{ std::move(project.m_Data) };
                    const auto load_project_work{
                        [=, &project]()
                        {
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

    const auto set_images_folder{
        [=, this, &project]()
        {
            if (const auto new_image_dir{ OpenFolderDialog(".") })
            {
                if (new_image_dir != project.m_Data.m_ImageDir)
                {
                    const auto old_image_dir{ std::move(project.m_Data.m_ImageDir) };

                    project.m_Data.m_ImageDir = std::move(new_image_dir).value();
                    project.m_Data.m_CropDir = project.m_Data.m_ImageDir / "crop";
                    project.m_Data.m_UncropDir = project.m_Data.m_ImageDir / "uncrop";
                    project.m_Data.m_ImageCache = project.m_Data.m_CropDir / "preview.cache";

                    project.Init();

                    ImageDirChanged(old_image_dir, project.m_Data.m_ImageDir);
                }
            }
        }
    };

    const auto open_images_folder{
        [=, &project]()
        {
            OpenFolder(project.m_Data.m_ImageDir);
        }
    };

    const auto render_alignment{
        [this, &project]()
        {
            auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
            GenericPopup render_align_window{ main_window, "Rendering alignment PDF..." };

            bool do_error_toast{ false };
            const auto render_work{
                [=, &project, &render_align_window, &do_error_toast]()
                {
                    const auto uninstall_log_hook{ render_align_window.InstallLogHook() };
                    try
                    {
                        const auto file_path{ GenerateTestPdf(project) };
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

    QObject::connect(render_button,
                     &QPushButton::clicked,
                     this,
                     render);
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
    QObject::connect(set_images_button,
                     &QPushButton::clicked,
                     this,
                     set_images_folder);
    QObject::connect(open_images_button,
                     &QPushButton::clicked,
                     this,
                     open_images_folder);
    QObject::connect(render_alignment_button,
                     &QPushButton::clicked,
                     this,
                     render_alignment);

    m_CropperProgressBar = cropper_progress_bar;
    m_RenderButton = render_button;

    // Just to set the right default text
    RenderBackendChanged();
}

void ActionsWidget::CropperWorking()
{
    m_CropperProgressBar->setVisible(true);
    m_CropperProgressBar->setValue(0);
    m_RenderButton->setVisible(false);
}

void ActionsWidget::CropperDone()
{
    m_CropperProgressBar->setVisible(false);
    m_RenderButton->setVisible(true);
}

void ActionsWidget::CropperProgress(float progress)
{
    const int progress_whole{ static_cast<int>(progress * c_ProgressBarResolution) };
    m_CropperProgressBar->setValue(progress_whole);
}

void ActionsWidget::RenderBackendChanged()
{
    switch (g_Cfg.m_Backend)
    {
    case PdfBackend::PoDoFo:
        m_RenderButton->setText("Render PDF");
        break;
    case PdfBackend::Png:
        m_RenderButton->setText("Render PNG");
    }
}
