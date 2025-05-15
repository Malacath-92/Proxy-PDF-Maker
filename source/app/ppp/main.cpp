#include <fmt/format.h>

#include <QApplication>
#include <QPushButton>

#include <QtPlugin>
#ifdef WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(__APPLE__)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#else
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#endif

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/cropper_thread_router.hpp>
#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/style.hpp>

#include <ppp/util/log.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_actions.hpp>
#include <ppp/ui/widget_card_options.hpp>
#include <ppp/ui/widget_global_options.hpp>
#include <ppp/ui/widget_guides_options.hpp>
#include <ppp/ui/widget_options_area.hpp>
#include <ppp/ui/widget_print_options.hpp>
#include <ppp/ui/widget_print_preview.hpp>
#include <ppp/ui/widget_scroll_area.hpp>

int main(int argc, char** argv)
{
#ifdef WIN32
    {
        static constexpr char locale_name[]{ ".utf-8" };
        std::setlocale(LC_ALL, locale_name);
        std::locale::global(std::locale(locale_name));
    }
#endif

    Log::RegisterThreadName("MainThread");

    LogFlags log_flags{
        LogFlags::Console |
        LogFlags::File |
        LogFlags::FatalQuit |
        LogFlags::DetailFile |
        LogFlags::DetailLine |
        LogFlags::DetailColumn |
        LogFlags::DetailThread |
        LogFlags::DetailStacktrace
    };
    Log main_log{ log_flags, Log::m_MainLogName };

    PrintProxyPrepApplication app{ argc, argv };
    SetStyle(app, app.GetTheme());

    Project project{};
    project.Load(app.GetProjectPath());

    Cropper cropper{ [&app](std::string_view cube_name)
                     {
                         return GetCubeImage(app, cube_name);
                     },
                     project };
    CardProvider card_provider{ project };

    QObject::connect(&card_provider, &CardProvider::CardAdded, &cropper, &Cropper::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &cropper, &Cropper::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardModified, &cropper, &Cropper::CardModified);

    QObject::connect(&card_provider, &CardProvider::CardAdded, &project, &Project::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &project, &Project::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &project, &Project::CardRenamed);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &cropper, &Cropper::CardRenamed);

    auto* scroll_area{ new CardScrollArea{ project } };
    auto* print_preview{ new PrintPreview{ project } };
    auto* tabs{ new MainTabs{ project, scroll_area, print_preview } };

    auto* actions{ new ActionsWidget{ app, project } };
    auto* print_options{ new PrintOptionsWidget{ project } };
    auto* guides_options{ new GuidesOptionsWidget{ project } };
    auto* card_options{ new CardOptionsWidget{ project } };
    auto* global_options{ new GlobalOptionsWidget{ app, project } };

    auto* options_area{
        new OptionsAreaWidget{
            actions,
            print_options,
            guides_options,
            card_options,
            global_options,
        },
    };

    auto* main_window{
        new PrintProxyPrepMainWindow{
            tabs,
            options_area,
        },
    };

    QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, &project, &Project::EnsureOutputFolder);
    QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, &project, &Project::EnsureOutputFolder);
    QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, &project, &Project::EnsureOutputFolder);
    QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChanged, &project, &Project::EnsureOutputFolder);

    // TODO: More fine-grained control
    QObject::connect(&card_provider, &CardProvider::CardAdded, scroll_area, std::bind_front(&CardScrollArea::Refresh, scroll_area, std::ref(project)));
    QObject::connect(&card_provider, &CardProvider::CardRemoved, scroll_area, std::bind_front(&CardScrollArea::Refresh, scroll_area, std::ref(project)));
    QObject::connect(&card_provider, &CardProvider::CardRenamed, scroll_area, std::bind_front(&CardScrollArea::Refresh, scroll_area, std::ref(project)));

    CropperThreadRouter cropper_router{ project };
    {
        QObject::connect(&cropper_router, &CropperThreadRouter::NewProjectOpenedDiff, &cropper, &Cropper::ClearCropWork);
        QObject::connect(&cropper_router, &CropperThreadRouter::ImageDirChangedDiff, &cropper, &Cropper::ClearCropWork);
        QObject::connect(&cropper_router, &CropperThreadRouter::CardSizeChangedDiff, &cropper, &Cropper::ClearCropWork);
        QObject::connect(&cropper_router, &CropperThreadRouter::BleedChangedDiff, &cropper, &Cropper::ClearCropWork);

        QObject::connect(&cropper_router, &CropperThreadRouter::BasePreviewWidthChangedDiff, &cropper, &Cropper::ClearPreviewWork);

        QObject::connect(&cropper_router, &CropperThreadRouter::NewProjectOpenedDiff, &cropper, &Cropper::NewProjectOpenedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::ImageDirChangedDiff, &cropper, &Cropper::ImageDirChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::CardSizeChangedDiff, &cropper, &Cropper::CardSizeChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::BleedChangedDiff, &cropper, &Cropper::BleedChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::EnableUncropChangedDiff, &cropper, &Cropper::EnableUncropChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::ColorCubeChangedDiff, &cropper, &Cropper::ColorCubeChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::BasePreviewWidthChangedDiff, &cropper, &Cropper::BasePreviewWidthChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::MaxDPIChangedDiff, &cropper, &Cropper::MaxDPIChangedDiff);
    }

    {
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, &cropper_router, &CropperThreadRouter::NewProjectOpened);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, &cropper_router, &CropperThreadRouter::ImageDirChanged);
        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, &cropper_router, &CropperThreadRouter::CardSizeChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, &cropper_router, &CropperThreadRouter::BleedChanged);

        QObject::connect(main_window, &PrintProxyPrepMainWindow::EnableUncropChanged, &cropper_router, &CropperThreadRouter::EnableUncropChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChanged, &cropper_router, &CropperThreadRouter::ColorCubeChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BasePreviewWidthChanged, &cropper_router, &CropperThreadRouter::BasePreviewWidthChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::MaxDPIChanged, &cropper_router, &CropperThreadRouter::MaxDPIChanged);
    }

    {
        // Sequence refreshing of cards after cleanup of cropper
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, &card_provider, &CardProvider::NewProjectOpened);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, &card_provider, &CardProvider::ImageDirChanged);
        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, &card_provider, &CardProvider::CardSizeChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, &card_provider, &CardProvider::BleedChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::EnableUncropChanged, &card_provider, &CardProvider::EnableUncropChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChanged, &card_provider, &CardProvider::ColorCubeChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BasePreviewWidthChanged, &card_provider, &CardProvider::BasePreviewWidthChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::MaxDPIChanged, &card_provider, &CardProvider::MaxDPIChanged);
    }

    {
        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideEnabledChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideDefaultChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::DisplayColumnsChanged, scroll_area, &CardScrollArea::Refresh);
    }

    {
        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::CornerWeightChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideDefaultChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideOffsetChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChanged, print_preview, &PrintPreview::Refresh);
    }

    {
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, print_options, &PrintOptionsWidget::NewProjectOpened);

        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::PageSizeChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::MarginsChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::CardLayoutChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::OrientationChanged, print_preview, &PrintPreview::Refresh);
    }

    {
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, guides_options, &GuidesOptionsWidget::NewProjectOpened);

        QObject::connect(guides_options, &GuidesOptionsWidget::ExactGuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::GuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::GuidesColorChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::BacksideGuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
    }

    {
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, print_options, &PrintOptionsWidget::NewProjectOpened);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, print_options, &PrintOptionsWidget::BleedChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BaseUnitChanged, print_options, &PrintOptionsWidget::BaseUnitChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::RenderBackendChanged, print_options, &PrintOptionsWidget::RenderBackendChanged);
    }

    {
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, card_options, &CardOptionsWidget::NewProjectOpened);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, card_options, &CardOptionsWidget::ImageDirChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BaseUnitChanged, card_options, &CardOptionsWidget::BaseUnitChanged);
    }

    app.SetMainWindow(main_window);
    main_window->show();

    // Write preview to project and forward to widgets
    QObject::connect(&cropper, &Cropper::PreviewUpdated, &project, &Project::SetPreview);

    // Enable and disable Render button
    QObject::connect(&cropper, &Cropper::CropWorkStart, actions, &ActionsWidget::CropperWorking);
    QObject::connect(&cropper, &Cropper::CropWorkDone, actions, &ActionsWidget::CropperDone);
    QObject::connect(&cropper, &Cropper::CropProgress, actions, &ActionsWidget::CropperProgress);

    // Write preview cache to file
    QObject::connect(&cropper, &Cropper::PreviewWorkDone, &project, &Project::CropperDone);

    cropper.Start();
    card_provider.Start();

    const int return_code{ app.exec() };
    project.Dump(app.GetProjectPath());
    return return_code;
}
