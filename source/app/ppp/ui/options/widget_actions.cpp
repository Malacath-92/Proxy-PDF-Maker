#include <ppp/ui/options/widget_actions.hpp>

#include <ranges>

#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedLayout>
#include <QStackedWidget>

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

ActionsWidget::ActionsWidget(Project& project)
{
    TRACY_AUTO_SCOPE();

    setObjectName("Actions");

    auto* set_images_button{ new QPushButton{ "Set Image Folder" } };
    auto* open_images_button{ new QPushButton{ "Open Images" } };
    auto* render_button{ new QPushButton{ "Render PDF" } };

    auto* cropper_progress_bar{ new QProgressBar };
    cropper_progress_bar->setAlignment(Qt::AlignCenter);
    cropper_progress_bar->setFormat("Processing...");
    cropper_progress_bar->setToolTip("Wait for processing to finish to render your project.");
    cropper_progress_bar->setVisible(false);
    cropper_progress_bar->setRange(0, c_ProgressBarResolution);

    {
        auto policy{ render_button->sizePolicy() };
        policy.setRetainSizeWhenHidden(true);
        policy.setVerticalPolicy(QSizePolicy::Preferred);
        policy.setHorizontalPolicy(QSizePolicy::Ignored);
        render_button->setSizePolicy(policy);
    }

    {
        auto policy{ cropper_progress_bar->sizePolicy() };
        policy.setRetainSizeWhenHidden(true);
        policy.setVerticalPolicy(QSizePolicy::Ignored);
        policy.setHorizontalPolicy(QSizePolicy::Preferred);
        cropper_progress_bar->setSizePolicy(policy);
    }

    auto* render_cropper_container{ new QStackedWidget };
    render_cropper_container->addWidget(render_button);
    render_cropper_container->addWidget(cropper_progress_bar);
    render_cropper_container->setCurrentWidget(render_button);

    auto* layout{ new QHBoxLayout };
    layout->addWidget(open_images_button);
    layout->addWidget(set_images_button);
    layout->addWidget(render_cropper_container);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    const auto render{
        [=, this, &project]()
        {
            TRACY_AUTO_SCOPE();

            auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
            GenericPopup render_window{ main_window, "Rendering PDF..." };

            bool do_error_toast{ false };
            const auto render_work{
                [=, &project, &render_window, &do_error_toast]()
                {
                    TRACY_AUTO_SCOPE();

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

    const auto set_images_folder{
        [=, this, &project]()
        {
            TRACY_AUTO_SCOPE();
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

    QObject::connect(render_button,
                     &QPushButton::clicked,
                     this,
                     render);
    QObject::connect(set_images_button,
                     &QPushButton::clicked,
                     this,
                     set_images_folder);
    QObject::connect(open_images_button,
                     &QPushButton::clicked,
                     this,
                     open_images_folder);

    m_RenderCropperContainer = render_cropper_container;
    m_CropperProgressBar = cropper_progress_bar;
    m_RenderButton = render_button;

    // Just to set the right default text
    RenderBackendChanged();
}

void ActionsWidget::CropperWorking()
{
    m_RenderCropperContainer->setCurrentWidget(m_CropperProgressBar);
    m_CropperProgressBar->setValue(0);
}

void ActionsWidget::CropperDone()
{
    m_RenderCropperContainer->setCurrentWidget(m_RenderButton);
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
