#include <optional>

#include <fmt/format.h>

#include <QApplication>
#include <QDesktopServices>
#include <QLayout>
#include <QMessageBox>
#include <QThreadPool>

#include <QtPlugin>
#ifdef WIN32
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(__APPLE__)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#else
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#endif

Q_IMPORT_PLUGIN(QTlsBackendOpenSSL)

Q_IMPORT_PLUGIN(QSvgIconPlugin)

#include <fmt/chrono.h>

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/auto_update.hpp>
#include <ppp/cubes.hpp>
#include <ppp/data_migration.hpp>
#include <ppp/style.hpp>
#include <ppp/version_check.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/ui/view_models/options/view_model_actions.hpp>
#include <ppp/ui/view_models/options/view_model_card_options.hpp>
#include <ppp/ui/view_models/options/view_model_global_options.hpp>
#include <ppp/ui/view_models/options/view_model_guides_options.hpp>
#include <ppp/ui/view_models/options/view_model_print_options.hpp>
#include <ppp/ui/view_models/options/view_model_project_options.hpp>
#include <ppp/ui/view_models/util.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/options/widget_actions.hpp>
#include <ppp/ui/options/widget_card_options.hpp>
#include <ppp/ui/options/widget_global_options.hpp>
#include <ppp/ui/options/widget_guides_options.hpp>
#include <ppp/ui/options/widget_options_area.hpp>
#include <ppp/ui/options/widget_print_options.hpp>
#include <ppp/ui/options/widget_project_options.hpp>
#include <ppp/ui/popups/popups.hpp>
#include <ppp/ui/preview/widget_print_preview.hpp>
#include <ppp/ui/widget_card_area.hpp>

#include <ppp/plugins/plugin_interface.hpp>

#include <ppp/profile/profile.hpp>

class PluginRouter : public PluginInterface
{
    virtual QWidget* Widget() override
    {
        return nullptr;
    }
};

void Reboot()
{
    QProcess::startDetached(
        QApplication::arguments()[0],
        QApplication::arguments().mid(1));
    QApplication::exit();
}

int main(int argc, char** argv)
{
    TRACY_WAIT_CONNECT();
    TRACY_AUTO_SCOPE();

#ifdef WIN32
    {
        static constexpr char c_LocaleName[]{ ".utf-8" };
        std::setlocale(LC_ALL, c_LocaleName);
        std::locale::global(std::locale(c_LocaleName));
    }
#endif

    PrintProxyPrepApplication app{ argc, argv };

    Log::RegisterThreadName("MainThread");
    Log::SetLogOutputFolder(app.GetCacheFolder() / "logs");

    LogFlags log_flags{
        LogFlags::Console |
        LogFlags::File |
        LogFlags::DetailFile |
        LogFlags::DetailLine |
        LogFlags::DetailColumn |
        LogFlags::DetailThread |
        LogFlags::DetailStacktrace
    };

    Log main_log{ log_flags, Log::c_MainLogName };

    app.LoadState();
    SetStyle(app.GetTheme());

    MigrateConfigFromCwd("config.ini", app.GetConfigFolder());
    const auto config_path{ app.GetConfigFolder() / "config.ini" };

    if constexpr (PPP_RELEASE_BUILD)
    {
        const auto cwd{ fs::absolute(".") };

        if (!app.HasMigratedProjectsFromCwd())
        {
            MigrateProjectsFromCwd(app.GetProjectsFolder());
            app.SetHasMigratedProjectsFromCwd(true);
        }

        if (!app.HasMigratedResourcesFromCwd())
        {
            MigrateResourcesFromCwd(app.GetDataFolder());
            app.SetHasMigratedResourcesFromCwd(true);
        }
    }

    Config config{};
    config.Load(config_path, app.GetCardSvgsFolder());
    const auto ideal_thread_count{ static_cast<uint32_t>(QThread::idealThreadCount()) };
    if (config.m_MaxWorkerThreads >= ideal_thread_count)
    {
        config.m_MaxWorkerThreads = ideal_thread_count - 2;
    }

    main_log.InstallHook(
        [](const Log::DetailInformation&, Log::LogLevel level, std::string_view message)
        {
            if (level == Log::LogLevel::Fatal)
            {
                QMessageBox::critical(nullptr,
                                      "Fatal Error",
                                      ToQString(message));
            }
        });

    {
        const std::span args{ argv, static_cast<size_t>(argc) };

        try
        {
            if (auto auto_update_phase{ ResolveAutoUpdatePhase(args) })
            {
                if (auto_update_phase(args))
                {
                    return 0;
                }
            }
            else
            {
                // Resource used in the static lib, but needs to be initialized
                // in the executable code
                Q_INIT_RESOURCE(install_manifest_resources);

                switch (AutoUpdateTryInitialize(args))
                {
                case AutoUpdateConclusion::Initiated:
                    return 0;
                case AutoUpdateConclusion::Error:
                    return 1;
                default:
                    break;
                }
            }
        }
        catch (std::exception& e)
        {
            LogFatal("Executing auto-update failed: {}\n", e.what());
            return 1;
        }
    }

    MigrateOldConfigDefaults(app, config);
    config.Save(config_path);

#ifdef PPP_DEBUG_CHILDLESS_WIDGETS
    class ParentCheckFilter : public QObject
    {
        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (event->type() == QEvent::Show)
            {
                QWidget* w = qobject_cast<QWidget*>(obj);
                if (w && !w->parentWidget())
                {
                    qDebug() << "Top-level widget shown:" << w << w->metaObject()->className();
                }
            }
            return QObject::eventFilter(obj, event);
        }
    };

    ParentCheckFilter filter{};
    app.installEventFilter(&filter);
#endif

    Project project{ config, app.GetProjectsFolder(), app.GetBasePdfsFolder() };
    const bool project_load_success{ project.Load(app.GetProjectPath()) };

    const auto project_backup_folder{ g_ExeDir / "_project_backup" };
    if (!project_load_success && fs::exists(app.GetProjectPath()))
    {
        if (!fs::exists(project_backup_folder))
        {
            fs::create_directories(project_backup_folder);
        }
        const auto backup_path{ GetNextVersionedPath(project_backup_folder / "proj.json") };
        fs::copy_file(app.GetProjectPath(), backup_path);
    }

    Cropper cropper{ [](std::string_view cube_name)
                     { return GetCubeImage(cube_name); },
                     project,
                     config };
    CardProvider card_provider{ project };

    QObject::connect(&card_provider, &CardProvider::CardAdded, &project, &Project::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &project, &Project::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &project, &Project::CardRenamed);
    QObject::connect(&card_provider, &CardProvider::CardModified, &project, &Project::CardModified);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &cropper, &Cropper::CardRenamed);

    QObject::connect(&card_provider, &CardProvider::CardAdded, &cropper, &Cropper::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &cropper, &Cropper::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardModified, &cropper, &Cropper::CardModified);

    QObject::connect(&project, &Project::CardRotationChanged, &cropper, &Cropper::CardModified);
    QObject::connect(&project, &Project::CardBleedTypeChanged, &cropper, &Cropper::CardModified);
    QObject::connect(&project, &Project::CardBadAspectRatioHandlingChanged, &cropper, &Cropper::CardModified);

    auto* actions_view_model{ new ActionsViewModel{ project, config } };
    auto* project_options_view_model{ new ProjectOptionsViewModel{ project, config } };
    auto* print_options_view_model{ new PrintOptionsViewModel{ project, config } };
    auto* guides_options_view_model{ new GuidesOptionsViewModel{ project, config } };
    auto* card_options_view_model{ new CardOptionsViewModel{ project, config } };
    auto* global_options_view_model{ new GlobalOptionsViewModel{ config } };

    auto* actions{ new ActionsWidget{ actions_view_model } };
    auto* card_area{ new CardArea{ project, config.m_DisplayColumns } };
    auto* print_preview{ new PrintPreview{ project, config } };
    auto* tabs{ new MainTabs{ actions, card_area, print_preview } };

    auto* project_options{ new ProjectOptionsWidget{ project_options_view_model } };
    auto* print_options{ new PrintOptionsWidget{ print_options_view_model } };
    auto* guides_options{ new GuidesOptionsWidget{ guides_options_view_model } };
    auto* card_options{ new CardOptionsWidget{ card_options_view_model } };
    auto* global_options{ new GlobalOptionsWidget{ global_options_view_model } };

    PluginRouter plugin_router{};
    QObject::connect(&plugin_router, &PluginRouter::PauseCropper, &cropper, &Cropper::PauseWork);
    QObject::connect(&plugin_router, &PluginRouter::UnpauseCropper, &cropper, &Cropper::RestartWork);
    QObject::connect(&plugin_router, &PluginRouter::RefreshCardGrid, card_area, &CardArea::FullRefresh);

    QObject::connect(
        &plugin_router,
        &PluginRouter::SetCardSizeChoice,
        &project,
        &Project::SetCardSizeChoice);
    QObject::connect(
        &plugin_router,
        &PluginRouter::SetEnableBackside,
        &project,
        &Project::SetBacksideEnabled);
    QObject::connect(
        &plugin_router,
        &PluginRouter::SetBacksideAutoPattern,
        &project,
        &Project::SetBacksideAutoPattern);

    auto* options_area{
        new OptionsAreaWidget{
            project,
            config,
            plugin_router,
            project_options,
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
            config,
        },
    };

    {
        auto update_max_display_columns{
            [&]()
            {
                TRACY_AUTO_SCOPE();
                TRACY_SCOPE_NAME(find_maximum_number_columns);

                const auto* primary_screen{ qApp->primaryScreen() };
                const auto geometry{ primary_screen->availableGeometry() };
                const auto available_width{ geometry.width() };

                const auto options_width{ options_area->width() };
                const auto window_width_margins{
                    main_window->contentsMargins().left() +
                    main_window->contentsMargins().right()
                };
                const auto window_spacing{ main_window->layout()->spacing() };
                const auto tabs_leftover_width{
                    available_width -
                    options_width -
                    window_width_margins -
                    window_spacing
                };
                const auto maximum_columns{
                    tabs->MaximumColumnsFromAvailableWidth(tabs_leftover_width)
                };

                config.SetMaxDisplayColumns(maximum_columns);
            }
        };

        QTimer::singleShot(100,
                           main_window,
                           update_max_display_columns);
        QObject::connect(&app,
                         &QApplication::primaryScreenChanged,
                         main_window,
                         update_max_display_columns);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_external_cards);

        // Creates the card-info correctly
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDropped, &project, &Project::AddExternalCard);

        // Notify user that we couldn't add this image
        QObject::connect(&project, &Project::FailedAddingExternalCard, main_window, &PrintProxyPrepMainWindow::ImageDropRejected);

        // Starts a watch on this file and forwards the relevant info to other widgets and systems
        QObject::connect(&project, &Project::ExternalCardAdded, &card_provider, &CardProvider::ExternalCardAdded);
        QObject::connect(&project, &Project::ExternalCardRemoved, &card_provider, &CardProvider::ExternalCardRemoved);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_app);
        QObject::connect(options_area, &OptionsAreaWidget::SetObjectVisibility, &app, &PrintProxyPrepApplication::SetObjectVisibility);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_actions_view_model);

#define FORWARD_SIGNAL_FROM_CONFIG(sig) \
    FORWARD_SIGNAL_FROM_TO(config, *actions_view_model, sig)

        FORWARD_SIGNAL_FROM_CONFIG(PdfBackendChanged);

#undef FORWARD_SIGNAL_FROM_CONFIG
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_print_options_view_model);

#define FORWARD_SIGNAL_FROM_PROJECT(sig) \
    FORWARD_SIGNAL_FROM_TO(project, *print_options_view_model, sig)

        FORWARD_SIGNAL_FROM_PROJECT(OutputFilenameChanged);
        FORWARD_SIGNAL_FROM_PROJECT(PageHeaderEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CardSizeChoiceChanged);
        FORWARD_SIGNAL_FROM_PROJECT(PageSizeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(PageSizeChoiceChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BasePdfChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CardsSizeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(PageMarginsModeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(PageMarginsChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CardOrientationChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CardsLayoutVerticalChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CardsLayoutHorizontalChanged);
        FORWARD_SIGNAL_FROM_PROJECT(PageOrientationChanged);
        FORWARD_SIGNAL_FROM_PROJECT(FlipPageOnChanged);

#undef FORWARD_SIGNAL_FROM_PROJECT

#define FORWARD_SIGNAL_FROM_CONFIG(sig) \
    FORWARD_SIGNAL_FROM_TO(config, *print_options_view_model, sig)

        FORWARD_SIGNAL_FROM_CONFIG(AdvancedModeChanged);
        FORWARD_SIGNAL_FROM_CONFIG(BaseUnitChanged);
        FORWARD_SIGNAL_FROM_CONFIG(AvailableCardSizesChanged);
        FORWARD_SIGNAL_FROM_CONFIG(AvailablePageSizesChanged);

#undef FORWARD_SIGNAL_FROM_CONFIG
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_guides_options_view_model);

#define FORWARD_SIGNAL_FROM_PROJECT(sig) \
    FORWARD_SIGNAL_FROM_TO(project, *guides_options_view_model, sig)

        FORWARD_SIGNAL_FROM_PROJECT(ExportExactGuidesChanged);
        FORWARD_SIGNAL_FROM_PROJECT(GuidesEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideGuidesEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CornerGuidesEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CrossGuidesEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(ExtendedGuidesEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(GuidesColorAChanged);
        FORWARD_SIGNAL_FROM_PROJECT(GuidesColorBChanged);
        FORWARD_SIGNAL_FROM_PROJECT(GuidesOffsetChanged);
        FORWARD_SIGNAL_FROM_PROJECT(GuidesLengthChanged);
        FORWARD_SIGNAL_FROM_PROJECT(GuidesThicknessChanged);

        FORWARD_SIGNAL_FROM_PROJECT(BacksideEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BleedEdgeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideEnabledChanged);

#undef FORWARD_SIGNAL_FROM_PROJECT

#define FORWARD_SIGNAL_FROM_CONFIG(sig) \
    FORWARD_SIGNAL_FROM_TO(config, *guides_options_view_model, sig)

        FORWARD_SIGNAL_FROM_CONFIG(AdvancedModeChanged);
        FORWARD_SIGNAL_FROM_CONFIG(BaseUnitChanged);

#undef FORWARD_SIGNAL_FROM_CONFIG
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_card_options_view_model);

#define FORWARD_SIGNAL_FROM_PROJECT(sig) \
    FORWARD_SIGNAL_FROM_TO(project, *card_options_view_model, sig)

        FORWARD_SIGNAL_FROM_PROJECT(BacksideEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(SeparateBacksidesEnabledChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideDefaultChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideOffsetChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideRotationChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideExtraBleedEdgeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BacksideAutoPatternChanged);
        FORWARD_SIGNAL_FROM_PROJECT(BleedEdgeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(EnvelopeBleedEdgeChanged);
        FORWARD_SIGNAL_FROM_PROJECT(SpacingChanged);
        FORWARD_SIGNAL_FROM_PROJECT(SpacingLinkedChanged);
        FORWARD_SIGNAL_FROM_PROJECT(CornersChanged);
        FORWARD_SIGNAL_FROM_PROJECT(ImageDirChanged);

#undef FORWARD_SIGNAL_FROM_PROJECT

#define FORWARD_SIGNAL_FROM_CONFIG(sig) \
    FORWARD_SIGNAL_FROM_TO(config, *card_options_view_model, sig)

        FORWARD_SIGNAL_FROM_CONFIG(AdvancedModeChanged);
        FORWARD_SIGNAL_FROM_CONFIG(BaseUnitChanged);

#undef FORWARD_SIGNAL_FROM_CONFIG
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_global_options_view_model);

#define FORWARD_SIGNAL_FROM_CONFIG(sig) \
    FORWARD_SIGNAL_FROM_TO(config, *global_options_view_model, sig)

        FORWARD_SIGNAL_FROM_CONFIG(AdvancedModeChanged);
        FORWARD_SIGNAL_FROM_CONFIG(NoCropModeChanged);
        FORWARD_SIGNAL_FROM_CONFIG(CheckVersionOnStartupChanged);
        FORWARD_SIGNAL_FROM_CONFIG(ToastTimeoutMSChanged);
        FORWARD_SIGNAL_FROM_CONFIG(BasePreviewWidthChanged);
        FORWARD_SIGNAL_FROM_CONFIG(MaxDPIChanged);
        FORWARD_SIGNAL_FROM_CONFIG(CardOrderChanged);
        FORWARD_SIGNAL_FROM_CONFIG(CardOrderDirectionChanged);
        FORWARD_SIGNAL_FROM_CONFIG(MaxWorkerThreadsChanged);
        FORWARD_SIGNAL_FROM_CONFIG(DisplayColumnsChanged);
        FORWARD_SIGNAL_FROM_CONFIG(MaxDisplayColumnsChanged);
        FORWARD_SIGNAL_FROM_CONFIG(ColorCubeChanged);
        FORWARD_SIGNAL_FROM_CONFIG(VersionOutputChanged);
        FORWARD_SIGNAL_FROM_CONFIG(PdfBackendChanged);
        FORWARD_SIGNAL_FROM_CONFIG(ImageCompressionChanged);
        FORWARD_SIGNAL_FROM_CONFIG(PngCompressionChanged);
        FORWARD_SIGNAL_FROM_CONFIG(JpgQualityChanged);
        FORWARD_SIGNAL_FROM_CONFIG(BaseUnitChanged);

#undef FORWARD_SIGNAL_FROM_CONFIG
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_project);

        QObject::connect(&config, &Config::NoCropModeChanged, &project, &Project::EnsureOutputFolder);
        QObject::connect(&config, &Config::ColorCubeChanged, &project, &Project::EnsureOutputFolder);

        QObject::connect(&config, &Config::CardOrderChanged, &project, &Project::CardOrderChanged);
        QObject::connect(&config, &Config::CardOrderDirectionChanged, &project, &Project::CardOrderDirectionChanged);

        QObject::connect(&config, &Config::AvailableCardSizesChanged, &project, &Project::AvailableCardSizesChanged);
        QObject::connect(&config, &Config::AvailablePageSizesChanged, &project, &Project::AvailablePageSizesChanged);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_cropper);

        QObject::connect(&project, &Project::ImageDirChanged, &cropper, &Cropper::CropDirChanged);
        QObject::connect(&project, &Project::NewProjectOpened, &cropper, &Cropper::CropDirChanged);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_card_provider);

        // Sequence refreshing of cards after cleanup of cropper
        QObject::connect(&project, &Project::ImageDirChanged, &card_provider, &CardProvider::ImageDirChanged);
        QObject::connect(&project, &Project::NewProjectOpened, &card_provider, &CardProvider::NewProjectOpened);
        QObject::connect(&project, &Project::CardSizeChanged, &card_provider, &CardProvider::CardSizeChanged);
        QObject::connect(&project, &Project::BleedEdgeChanged, &card_provider, &CardProvider::BleedChanged);
        QObject::connect(&project, &Project::EnvelopeBleedEdgeChanged, &card_provider, &CardProvider::BleedChanged);
        QObject::connect(&project, &Project::BacksideExtraBleedEdgeChanged, &card_provider, &CardProvider::BacksideExtraBleedChanged);
        QObject::connect(&config, &Config::ColorCubeChanged, &card_provider, &CardProvider::ColorCubeChanged);
        QObject::connect(&config, &Config::BasePreviewWidthChanged, &card_provider, &CardProvider::BasePreviewWidthChanged);
        QObject::connect(&config, &Config::NoCropModeChanged, &card_provider, &CardProvider::NoCropModeChanged);
        QObject::connect(&config, &Config::MaxDPIChanged, &card_provider, &CardProvider::MaxDPIChanged);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_card_area);

        QObject::connect(&card_provider, &CardProvider::CardAdded, card_area, &CardArea::CardAdded);
        QObject::connect(&card_provider, &CardProvider::CardRemoved, card_area, &CardArea::CardRemoved);
        QObject::connect(&card_provider, &CardProvider::CardRenamed, card_area, &CardArea::CardRenamed);

        QObject::connect(&project, &Project::CardVisibilityChanged, card_area, &CardArea::CardVisibilityChanged);

        QObject::connect(&project, &Project::ImageDirChanged, card_area, &CardArea::ImageDirChanged);
        QObject::connect(&project, &Project::NewProjectOpened, card_area, &CardArea::NewProjectOpened);
        QObject::connect(&project, &Project::BacksideEnabledChanged, card_area, &CardArea::BacksideEnabledChanged);
        QObject::connect(&project, &Project::BacksideDefaultChanged, card_area, &CardArea::BacksideDefaultChanged);
        QObject::connect(&project, &Project::CardBacksideChanged, card_area, &CardArea::FullRefresh);
        QObject::connect(&project, &Project::CardSizeChanged, card_area, &CardArea::CardSizeChanged);
        QObject::connect(&config, &Config::DisplayColumnsChanged, card_area, &CardArea::DisplayColumnsChanged);
        QObject::connect(&config, &Config::CardOrderChanged, card_area, &CardArea::CardOrderChanged);
        QObject::connect(&config, &Config::CardOrderDirectionChanged, card_area, &CardArea::CardOrderDirectionChanged);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_preview);

        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(&project, &Project::ImageDirChanged, print_preview, &PrintPreview::Refresh);

        QObject::connect(&project, &Project::NewProjectOpened, print_preview, &PrintPreview::Refresh);

        QObject::connect(&card_provider, &CardProvider::CardAdded, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&card_provider, &CardProvider::CardRemoved, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&card_provider, &CardProvider::CardRenamed, print_preview, &PrintPreview::RequestRefresh);

        QObject::connect(&project, &Project::CardSizeChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::PageSizeChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::PageOrientationChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::PageMarginsChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::CardsLayoutVerticalChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::CardsLayoutHorizontalChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::FlipPageOnChanged, print_preview, &PrintPreview::RequestRefresh);

        QObject::connect(&project, &Project::ExportExactGuidesChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::GuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::BacksideGuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::CornerGuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::CrossGuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::ExtendedGuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::GuidesColorAChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::GuidesColorBChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::GuidesOffsetChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::GuidesLengthChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::GuidesThicknessChanged, print_preview, &PrintPreview::RequestRefresh);

        QObject::connect(&project, &Project::BleedEdgeChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::EnvelopeBleedEdgeChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::SpacingChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::CornersChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::BacksideEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::BacksideDefaultChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::BacksideOffsetChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&project, &Project::BacksideExtraBleedEdgeChanged, print_preview, &PrintPreview::RequestRefresh);

        QObject::connect(&config, &Config::ColorCubeChanged, print_preview, &PrintPreview::RequestRefresh);
        QObject::connect(&config, &Config::CardOrderChanged, print_preview, &PrintPreview::CardOrderChanged);
        QObject::connect(&config, &Config::CardOrderDirectionChanged, print_preview, &PrintPreview::CardOrderDirectionChanged);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_new_project);

        QObject::connect(&app, &PrintProxyPrepApplication::ProjectPathChanged, project_options_view_model, &ProjectOptionsViewModel::ProjectPathChanged);
        QObject::connect(&app, &PrintProxyPrepApplication::ProjectPathChanged, main_window, &PrintProxyPrepMainWindow::ProjectPathChanged);

        QObject::connect(&project, &Project::NewProjectOpened, print_options_view_model, &PrintOptionsViewModel::NewProjectOpened);
        QObject::connect(&project, &Project::NewProjectOpened, guides_options_view_model, &GuidesOptionsViewModel::NewProjectOpened);
        QObject::connect(&project, &Project::NewProjectOpened, card_options_view_model, &CardOptionsViewModel::NewProjectOpened);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_global_options);

        QObject::connect(card_area, &CardArea::RequestOpenPluginsWindow, global_options_view_model, &GlobalOptionsViewModel::OpenPluginsWindow);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_options_area);

        QObject::connect(&config, &Config::PluginEnabled, options_area, &OptionsAreaWidget::PluginEnabled);
        QObject::connect(&config, &Config::PluginDisabled, options_area, &OptionsAreaWidget::PluginDisabled);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_resource_drag_and_drop);

        // Move user resources into the right folders
        QObject::connect(main_window, &PrintProxyPrepMainWindow::PdfDropped, &project, [&app](const auto& path)
                         { fs::copy(path, app.GetBasePdfsFolder(), fs::copy_options::overwrite_existing); });
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeDropped, &project, [&app](const auto& path)
                         { fs::copy(path, app.GetCubesFolder(), fs::copy_options::overwrite_existing); });
        QObject::connect(main_window, &PrintProxyPrepMainWindow::StyleDropped, &project, [&app](const auto& path)
                         { fs::copy(path, app.GetStylesFolder(), fs::copy_options::overwrite_existing); });
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ModelDropped, &project, [&app](const auto& path)
                         { fs::copy(path, app.GetUpscaleModelsFolder(), fs::copy_options::overwrite_existing); });
        QObject::connect(main_window,
                         &PrintProxyPrepMainWindow::SvgDropped,
                         &project,
                         [&app, &config](const auto& path)
                         {
                             const auto card_svgs_folder{ app.GetCardSvgsFolder() };
                             if (fs::absolute(path.parent_path()) != fs::absolute(card_svgs_folder))
                             {
                                 fs::copy(path, card_svgs_folder, fs::copy_options::overwrite_existing);
                             }

                             // Add a new card size
                             config.SvgCardSizeAdded(card_svgs_folder / path.filename());
                         });

        // Refresh corresponding widgets
        QObject::connect(main_window, &PrintProxyPrepMainWindow::PdfDropped, print_options_view_model, &PrintOptionsViewModel::BasePdfAdded);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeDropped, global_options_view_model, &GlobalOptionsViewModel::ColorCubeAdded);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::StyleDropped, global_options_view_model, &GlobalOptionsViewModel::StyleAdded);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_card_order);

        QObject::connect(
            print_preview,
            &PrintPreview::RestoreCardsOrder,
            &project,
            [&]()
            {
                project.RestoreCardsOrder();
                print_preview->RequestRefresh();
            },
            Qt::ConnectionType::QueuedConnection);
        QObject::connect(
            print_preview,
            &PrintPreview::ReorderCards,
            &project,
            [&](size_t from, size_t to)
            {
                if (project.ReorderCards(from, to))
                {
                    print_preview->RequestRefresh();
                }
                else
                {
                    // clang-formt off
                    QString message{
                        QString{
                            "Failed reordering cards, moving %1 to %2",
                        }
                            .arg(from)
                            .arg(to)
                    };
                    // clang-formt on
                    main_window->Toast(
                        ToastType::Error,
                        "Drag-and-Drop Error",
                        std::move(message));
                }
            },
            Qt::ConnectionType::QueuedConnection);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(show_main_window);

        app.SetMainWindow(main_window);
        main_window->show();
    }

    if (!project_load_success && !app.IsFirstStartup())
    {
        if (!fs::exists(app.GetProjectPath()))
        {
            main_window->Toast(
                ToastType::Warning,
                "Failed loading project",
                "The file didn't exist.");
        }
        else
        {
            main_window->Toast(
                ToastType::Warning,
                "Failed loading project",
                QString{ "Your project has been backed up <a style=\"color:CornflowerBlue\" href=\"file:///%1\">"
                         "here"
                         "</a>, please attach it if you report this bug. Press F1 to find the issues page." }
                    .arg(ToQString(project_backup_folder).replace(' ', "%20")),
                [&](const QString&)
                {
                    OpenFolder(project_backup_folder);
                });
        }
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(connect_signals_crop_work);

        // Write preview to project and forward to widgets
        // clang-format off
        QObject::connect(&cropper, &Cropper::PreviewUpdated, &project,
                         [&project](const fs::path& card_name, ImagePreview* preview, Image::Rotation rotation) {
                             project.SetPreview(card_name, std::move(*preview), rotation);
                             delete preview;
                         });
        // clang-format on

        // Enable and disable Render button
        QObject::connect(&cropper, &Cropper::CropWorkStart, actions_view_model, &ActionsViewModel::CropperWorking);
        QObject::connect(&cropper, &Cropper::CropWorkDone, actions_view_model, &ActionsViewModel::CropperDone);
        QObject::connect(&cropper, &Cropper::CropProgress, actions_view_model, &ActionsViewModel::CropperProgress);

        // Write preview cache to file
        QObject::connect(&cropper, &Cropper::PreviewWorkDone, &project, &Project::CropperDone);

        // Toast to user when crop work is done
        QObject::connect(&cropper,
                         &Cropper::CropWorkDone,
                         main_window,
                         [main_window](std::chrono::seconds time,
                                       uint32_t work_done,
                                       uint32_t work_skipped)
                         {
                             if (work_done == 0)
                             {
                                 return;
                             }

                             // clang-formt off
                             QString message{
                                 QString{
                                     "Took %1 seconds to "
                                     "crop %2 images.",
                                 }
                                     .arg(fmt::format("{}", time).c_str())
                                     .arg(work_done)
                             };
                             if (work_skipped > 0)
                             {
                                 message += QString{
                                     " An addtional %1 images were verified.",
                                 }
                                                .arg(work_skipped);
                             }
                             // clang-formt on
                             main_window->Toast(
                                 ToastType::Info,
                                 "Cropper finished",
                                 std::move(message));
                         });
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(set_max_worker_threads);

        auto apply_max_worker_threads{
            [&config]()
            {
                QThreadPool::globalInstance()->setMaxThreadCount(config.m_MaxWorkerThreads);
            }
        };
        apply_max_worker_threads();
        QObject::connect(&config, &Config::MaxWorkerThreadsChanged, main_window, apply_max_worker_threads);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(start_cropper_and_card_provider);

        cropper.Start();
        card_provider.Start();
    }

    if (config.m_CheckVersionOnStartup)
    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(check_version);

        if (auto new_version{ NewAvailableVersion() })
        {
            static constexpr char c_AutoUpdate[]{ "#auto-update" };
            static auto s_AutoUpdate{
                [main_window](std::string_view version)
                {
                    if (AutoUpdateDownloadRelease(version))
                    {
                        static constexpr char c_Restart[]{ "#restart" };
                        main_window->Toast(
                            ToastType::Info,
                            "Restart to Update",
                            QString{ "New version downloaded, <a style=\"color:CornflowerBlue\" href=\"%1\">"
                                     "restart app"
                                     "</a> to finish" }
                                .arg(c_Restart),
                            [=](const QString& /*link*/)
                            {
                                Reboot();
                            });
                    }
                }
            };
            main_window->Toast(
                ToastType::Info,
                "New version available",
                QString{ "<a style=\"color:CornflowerBlue\" href=\"%1\">"
                         "Download the new version %2 from GitHub"
                         "</a> or <a style=\"color:CornflowerBlue\" href=\"%3\">"
                         "Auto-Update"
                         "</a>" }
                    .arg(ReleaseURL(new_version.value()).c_str())
                    .arg(new_version.value().c_str())
                    .arg(c_AutoUpdate),
                [=](const QString& link)
                {
                    if (link == c_AutoUpdate)
                    {
                        try
                        {
                            s_AutoUpdate(new_version.value());
                        }
                        catch (const std::exception& e)
                        {
                            LogError("Error during Auto-Update: {}", e.what());
                            main_window->Toast(
                                ToastType::Error,
                                "Auto-Update Error",
                                "Failed downloading new version...");
                        }
                    }
                    else
                    {
                        QDesktopServices::openUrl(link);
                    }
                });
        }
    }

    const auto ret{
        []()
        {
            TRACY_AUTO_SCOPE();
            TRACY_SCOPE_NAME(QApplication::exec);
            return QApplication::exec();
        }()
    };

    config.Save(config_path);
    project.Dump(app.GetProjectPath());

    return ret;
}
