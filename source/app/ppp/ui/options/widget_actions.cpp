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

ActionsWidget::ActionsWidget(ActionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Actions");
    m_ViewModel.setParent(this);

    auto* set_images_button{ new QPushButton{ "Set Image Folder" } };
    auto* open_images_button{ new QPushButton{ "Open Images" } };
    m_RenderButton = new QPushButton{ "Render PDF" };

    m_CropperProgressBar = new QProgressBar;
    m_CropperProgressBar->setAlignment(Qt::AlignCenter);
    m_CropperProgressBar->setFormat("Processing...");
    m_CropperProgressBar->setToolTip("Wait for processing to finish to render your project.");
    m_CropperProgressBar->setVisible(false);
    m_CropperProgressBar->setRange(0, c_ProgressBarResolution);

    {
        auto policy{ m_RenderButton->sizePolicy() };
        policy.setRetainSizeWhenHidden(true);
        policy.setVerticalPolicy(QSizePolicy::Preferred);
        policy.setHorizontalPolicy(QSizePolicy::Ignored);
        m_RenderButton->setSizePolicy(policy);
    }

    {
        auto policy{ m_CropperProgressBar->sizePolicy() };
        policy.setRetainSizeWhenHidden(true);
        policy.setVerticalPolicy(QSizePolicy::Ignored);
        policy.setHorizontalPolicy(QSizePolicy::Preferred);
        m_CropperProgressBar->setSizePolicy(policy);
    }

    m_RenderCropperContainer = new QStackedWidget;
    m_RenderCropperContainer->addWidget(m_RenderButton);
    m_RenderCropperContainer->addWidget(m_CropperProgressBar);
    m_RenderCropperContainer->setCurrentWidget(m_RenderButton);

    auto* layout{ new QHBoxLayout };
    layout->addWidget(open_images_button);
    layout->addWidget(set_images_button);
    layout->addWidget(m_RenderCropperContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    QObject::connect(m_RenderButton,
                     &QPushButton::clicked,
                     this,
                     &ActionsWidget::RenderButtonPressed);
    QObject::connect(set_images_button,
                     &QPushButton::clicked,
                     this,
                     &ActionsWidget::SetImagesButtonPressed);
    QObject::connect(open_images_button,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     &ActionsViewModel::OpenImagesFolder);

    QObject::connect(&m_ViewModel,
                     &ActionsViewModel::CropperWorking,
                     this,
                     &ActionsWidget::CropperWorking);
    QObject::connect(&m_ViewModel,
                     &ActionsViewModel::CropperDone,
                     this,
                     &ActionsWidget::CropperDone);
    QObject::connect(&m_ViewModel,
                     &ActionsViewModel::CropperProgress,
                     this,
                     &ActionsWidget::CropperProgress);
    QObject::connect(&m_ViewModel,
                     &ActionsViewModel::PdfBackendChanged,
                     this,
                     &ActionsWidget::PdfBackendChanged);

    m_ViewModel.EmitDefaults();
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

void ActionsWidget::PdfBackendChanged(PdfBackend backend)
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
    if (const auto new_image_dir{ OpenFolderDialog(m_ViewModel.GetImageFolderBase()) })
    {
        m_ViewModel.SetImagesFolder(std::move(new_image_dir).value());
    }
}
