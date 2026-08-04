#include <ppp/ui/options/widget_actions.hpp>

#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedLayout>
#include <QStackedWidget>

#include <ppp/app.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups/new_project_popup.hpp>
#include <ppp/ui/popups/popups.hpp>

#include <ppp/ui/view_models/options/view_model_actions.hpp>

#include <ppp/profile/profile.hpp>

ActionsWidget::ActionsWidget(ActionsViewModel* view_model,
                             PdfBackend backend)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Actions");
    view_model->setParent(this);

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

    QObject::connect(render_button,
                     &QPushButton::clicked,
                     this,
                     &ActionsWidget::RenderButtonPressed);
    QObject::connect(set_images_button,
                     &QPushButton::clicked,
                     this,
                     &ActionsWidget::SetImagesButtonPressed);
    QObject::connect(open_images_button,
                     &QPushButton::clicked,
                     view_model,
                     &ActionsViewModel::OpenImagesFolder);

    QObject::connect(view_model,
                     &ActionsViewModel::CropperWorking,
                     this,
                     &ActionsWidget::CropperWorking);
    QObject::connect(view_model,
                     &ActionsViewModel::CropperDone,
                     this,
                     &ActionsWidget::CropperDone);
    QObject::connect(view_model,
                     &ActionsViewModel::CropperProgress,
                     this,
                     &ActionsWidget::CropperProgress);
    QObject::connect(view_model,
                     &ActionsViewModel::RenderBackendChanged,
                     this,
                     &ActionsWidget::RenderBackendChanged);

    m_RenderCropperContainer = render_cropper_container;
    m_CropperProgressBar = cropper_progress_bar;
    m_RenderButton = render_button;

    // Just to set the right default text
    RenderBackendChanged(backend);
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

void ActionsWidget::RenderBackendChanged(PdfBackend backend)
{
    switch (backend)
    {
    case PdfBackend::PoDoFo:
        m_RenderButton->setText("Render PDF");
        break;
    case PdfBackend::Png:
        m_RenderButton->setText("Render PNG");
    }
}

void ActionsWidget::RenderButtonPressed() const
{
    TRACY_AUTO_SCOPE();

    auto* main_window{ static_cast<PrintProxyPrepMainWindow*>(window()) };
    GenericPopup render_window{ nullptr, "Rendering PDF..." };

    bool do_error_toast{ false };
    const auto render_work{
        [this, main_window, &render_window, &do_error_toast]()
        {
            TRACY_AUTO_SCOPE();

            const auto uninstall_log_hook{ render_window.InstallLogHook() };

            try
            {
                m_ViewModel.RenderDocument();
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

void ActionsWidget::SetImagesButtonPressed() const
{
    TRACY_AUTO_SCOPE();
    if (const auto new_image_dir{ OpenFolderDialog(".") })
    {
        m_ViewModel.SetImagesFolder(std::move(new_image_dir).value());
    }
}
