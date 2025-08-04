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

Q_IMPORT_PLUGIN(QTlsBackendOpenSSL)

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

#include <ppp/plugins/plugin_interface.hpp>

class PluginRouter : public PluginInterface
{
    virtual QWidget* Widget() override
    {
        return nullptr;
    }
};

int main(int argc, char** argv)
{
#ifdef WIN32
    {
        static constexpr char c_LocaleName[]{ ".utf-8" };
        std::setlocale(LC_ALL, c_LocaleName);
        std::locale::global(std::locale(c_LocaleName));
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
    Log main_log{ log_flags, Log::c_MainLogName };

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
    auto* tabs{ new MainTabs{ scroll_area, print_preview } };

    auto* actions{ new ActionsWidget{ app, project } };
    auto* print_options{ new PrintOptionsWidget{ project } };
    auto* guides_options{ new GuidesOptionsWidget{ project } };
    auto* card_options{ new CardOptionsWidget{ project } };
    auto* global_options{ new GlobalOptionsWidget{ app } };

    PluginRouter plugin_router{};
    QObject::connect(&plugin_router, &PluginRouter::PauseCropper, [&cropper]()
                     { cropper.PauseWork(); });
    QObject::connect(&plugin_router, &PluginRouter::UnpauseCropper, [&cropper]()
                     { cropper.RestartWork(); });
    QObject::connect(&plugin_router, &PluginRouter::RefreshCardGrid, scroll_area, &CardScrollArea::FullRefresh);

    auto* options_area{
        new OptionsAreaWidget{
            app,
            project,
            plugin_router,
            actions,
            print_options,
            guides_options,
            card_options,
            global_options,
        },
    };

    QObject::connect(options_area, &OptionsAreaWidget::SetObjectVisibility, &app, &PrintProxyPrepApplication::SetObjectVisibility);

    auto* main_window{
        new PrintProxyPrepMainWindow{
            tabs,
            options_area,
        },
    };

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, &project, &Project::EnsureOutputFolder);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, &project, &Project::EnsureOutputFolder);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, &project, &Project::EnsureOutputFolder);
        QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, &project, &Project::EnsureOutputFolder);
    }

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
        QObject::connect(&cropper_router, &CropperThreadRouter::ColorCubeChangedDiff, &cropper, &Cropper::ColorCubeChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::BasePreviewWidthChangedDiff, &cropper, &Cropper::BasePreviewWidthChangedDiff);
        QObject::connect(&cropper_router, &CropperThreadRouter::MaxDPIChangedDiff, &cropper, &Cropper::MaxDPIChangedDiff);
    }

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, &cropper_router, &CropperThreadRouter::NewProjectOpened);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, &cropper_router, &CropperThreadRouter::ImageDirChanged);
        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, &cropper_router, &CropperThreadRouter::CardSizeChanged);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, &cropper_router, &CropperThreadRouter::BleedChanged);

        QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, &cropper_router, &CropperThreadRouter::ColorCubeChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BasePreviewWidthChanged, &cropper_router, &CropperThreadRouter::BasePreviewWidthChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::MaxDPIChanged, &cropper_router, &CropperThreadRouter::MaxDPIChanged);
    }

    {
        // Sequence refreshing of cards after cleanup of cropper
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, &card_provider, &CardProvider::NewProjectOpened);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, &card_provider, &CardProvider::ImageDirChanged);
        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, &card_provider, &CardProvider::CardSizeChanged);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, &card_provider, &CardProvider::BleedChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, &card_provider, &CardProvider::ColorCubeChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BasePreviewWidthChanged, &card_provider, &CardProvider::BasePreviewWidthChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::MaxDPIChanged, &card_provider, &CardProvider::MaxDPIChanged);
    }

    {
        QObject::connect(&card_provider, &CardProvider::CardAdded, scroll_area, &CardScrollArea::CardAdded);
        QObject::connect(&card_provider, &CardProvider::CardRemoved, scroll_area, &CardScrollArea::CardRemoved);
        QObject::connect(&card_provider, &CardProvider::CardRenamed, scroll_area, &CardScrollArea::CardRenamed);

        QObject::connect(actions, &ActionsWidget::NewProjectOpened, scroll_area, &CardScrollArea::NewProjectOpened);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, scroll_area, &CardScrollArea::ImageDirChanged);
        QObject::connect(card_options, &CardOptionsWidget::BacksideEnabledChanged, scroll_area, &CardScrollArea::BacksideEnabledChanged);
        QObject::connect(card_options, &CardOptionsWidget::BacksideDefaultChanged, scroll_area, &CardScrollArea::BacksideDefaultChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::DisplayColumnsChanged, scroll_area, &CardScrollArea::DisplayColumnsChanged);
    }

    {
        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, print_preview, &PrintPreview::Refresh);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, print_preview, &PrintPreview::Refresh);

        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::PageSizeChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::MarginsChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::CardLayoutChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::OrientationChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(print_options, &PrintOptionsWidget::FlipOnChanged, print_preview, &PrintPreview::Refresh);

        QObject::connect(guides_options, &GuidesOptionsWidget::ExactGuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::GuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::BacksideGuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::CornerGuidesChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::CrossGuidesChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::ExtendedGuidesChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::GuidesColorChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::GuidesOffsetChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(guides_options, &GuidesOptionsWidget::GuidesLengthChanged, print_preview, &PrintPreview::Refresh);

        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(card_options, &CardOptionsWidget::SpacingChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(card_options, &CardOptionsWidget::CornersChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(card_options, &CardOptionsWidget::BacksideEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(card_options, &CardOptionsWidget::BacksideDefaultChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(card_options, &CardOptionsWidget::BacksideOffsetChanged, print_preview, &PrintPreview::Refresh);

        QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, print_preview, &PrintPreview::Refresh);
    }

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, print_options, &PrintOptionsWidget::NewProjectOpened);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, print_options, &PrintOptionsWidget::BleedChanged);
        QObject::connect(card_options, &CardOptionsWidget::SpacingChanged, print_options, &PrintOptionsWidget::SpacingChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BaseUnitChanged, print_options, &PrintOptionsWidget::BaseUnitChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::RenderBackendChanged, print_options, &PrintOptionsWidget::RenderBackendChanged);
    }

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, guides_options, &GuidesOptionsWidget::NewProjectOpened);
        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, guides_options, &GuidesOptionsWidget::CardSizeChanged);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, guides_options, &GuidesOptionsWidget::BleedChanged);
        QObject::connect(card_options, &CardOptionsWidget::BacksideEnabledChanged, guides_options, &GuidesOptionsWidget::BacksideEnabledChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BaseUnitChanged, guides_options, &GuidesOptionsWidget::BaseUnitChanged);
    }

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, card_options, &CardOptionsWidget::NewProjectOpened);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, card_options, &CardOptionsWidget::ImageDirChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BaseUnitChanged, card_options, &CardOptionsWidget::BaseUnitChanged);
    }

    {
        QObject::connect(global_options, &GlobalOptionsWidget::PluginEnabled, options_area, &OptionsAreaWidget::PluginEnabled);
        QObject::connect(global_options, &GlobalOptionsWidget::PluginDisabled, options_area, &OptionsAreaWidget::PluginDisabled);
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
