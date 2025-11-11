#include <fmt/format.h>

#include <QApplication>
#include <QThreadPool>

#include <QtPlugin>
#ifdef WIN32
#include <Windows.h>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(__APPLE__)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#else
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#endif

Q_IMPORT_PLUGIN(QTlsBackendOpenSSL)

#include <fmt/chrono.h>

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/style.hpp>
#include <ppp/version_check.hpp>

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

#include <ppp/cli/cli_main.hpp>

#include <ppp/pdf/generate.hpp>

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

    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
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
    SetStyle(app.GetTheme());

    auto apply_max_worker_threads{
        []()
        {
            QThreadPool::globalInstance()->setMaxThreadCount(g_Cfg.m_MaxWorkerThreads);
        }
    };
    apply_max_worker_threads();

    CommandLineOptions cli{ ParseCommandLine(argc, argv) };
    if (cli.m_HelpDisplayed)
    {
        return 0;
    }

    Project project{};
    if (cli.m_ProjectFile.has_value())
    {
        project.Load(cli.m_ProjectFile.value(),
                     cli.m_ProjectOverrides);
    }
    else if (cli.m_ProjectJson.has_value())
    {
        if (!project.LoadFromJson(cli.m_ProjectJson.value(),
                                  cli.m_ProjectOverrides))
        {
            LogError("Failed loading project from json-blob...");
        }
    }
    else
    {
        project.Load(app.GetProjectPath(),
                     cli.m_ProjectOverrides);
    }

    Cropper cropper{ [](std::string_view cube_name)
                     {
                         return GetCubeImage(cube_name);
                     },
                     project };
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

    if (!cli.m_NoUI)
    {
        auto* scroll_area{ new CardScrollArea{ project } };
        auto* print_preview{ new PrintPreview{ project } };
        auto* tabs{ new MainTabs{ scroll_area, print_preview } };

        auto* actions{ new ActionsWidget{ project } };
        auto* print_options{ new PrintOptionsWidget{ project } };
        auto* guides_options{ new GuidesOptionsWidget{ project } };
        auto* card_options{ new CardOptionsWidget{ project } };
        auto* global_options{ new GlobalOptionsWidget{} };

        PluginRouter plugin_router{};
        QObject::connect(&plugin_router, &PluginRouter::PauseCropper, [&cropper]()
                         { cropper.PauseWork(); });
        QObject::connect(&plugin_router, &PluginRouter::UnpauseCropper, [&cropper]()
                         { cropper.RestartWork(); });
        QObject::connect(&plugin_router, &PluginRouter::RefreshCardGrid, scroll_area, &CardScrollArea::FullRefresh);

        QObject::connect(
            &plugin_router,
            &PluginRouter::SetCardSizeChoice,
            [&](const std::string& card_size_choice)
            {
                if (g_Cfg.m_CardSizes.contains(card_size_choice) && card_size_choice != project.m_Data.m_CardSizeChoice)
                {
                    project.m_Data.m_CardSizeChoice = card_size_choice;
                    print_options->ExternalCardSizeChanged();
                }
            });
        QObject::connect(
            &plugin_router,
            &PluginRouter::SetEnableBackside,
            [&](bool enabled)
            {
                if (project.SetBacksideEnabled(enabled))
                {
                    card_options->BacksideEnabledChangedExternal();
                }
            });

        QObject::connect(
            &plugin_router,
            &PluginRouter::SetBacksideAutoPattern,
            [&](const std::string& pattern)
            {
                project.SetBacksideAutoPattern(pattern);
                card_options->BacksideAutoPatternChangedExternal(pattern);
            });

        auto* options_area{
            new OptionsAreaWidget{
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
            // Creates the card-info correctly
            QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDropped, &project, &Project::AddExternalCard);

            // Notify user that we couldn't add this image
            QObject::connect(&project, &Project::FailedAddingExternalCard, main_window, &PrintProxyPrepMainWindow::ImageDropRejected);

            // Starts a watch on this file and forwards the relevant info to other widgets and systems
            QObject::connect(&project, &Project::ExternalCardAdded, &card_provider, &CardProvider::ExternalCardAdded);
            QObject::connect(&project, &Project::ExternalCardRemoved, &card_provider, &CardProvider::ExternalCardRemoved);
        }

        {
            QObject::connect(actions, &ActionsWidget::NewProjectOpened, &project, &Project::EnsureOutputFolder);
            QObject::connect(actions, &ActionsWidget::ImageDirChanged, &project, &Project::EnsureOutputFolder);
            QObject::connect(card_options, &CardOptionsWidget::BleedChanged, &project, &Project::EnsureOutputFolder);
            QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, &project, &Project::EnsureOutputFolder);

            QObject::connect(global_options, &GlobalOptionsWidget::CardOrderChanged, &project, &Project::CardOrderChanged);
            QObject::connect(global_options, &GlobalOptionsWidget::CardOrderDirectionChanged, &project, &Project::CardOrderDirectionChanged);
        }

        {
            QObject::connect(actions, &ActionsWidget::NewProjectOpened, &cropper, &Cropper::CropDirChanged);
            QObject::connect(actions, &ActionsWidget::ImageDirChanged, &cropper, &Cropper::CropDirChanged);
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

            QObject::connect(&project, &Project::CardVisibilityChanged, scroll_area, &CardScrollArea::CardVisibilityChanged);

            QObject::connect(actions, &ActionsWidget::NewProjectOpened, scroll_area, &CardScrollArea::NewProjectOpened);
            QObject::connect(actions, &ActionsWidget::ImageDirChanged, scroll_area, &CardScrollArea::ImageDirChanged);
            QObject::connect(card_options, &CardOptionsWidget::BacksideEnabledChanged, scroll_area, &CardScrollArea::BacksideEnabledChanged);
            QObject::connect(card_options, &CardOptionsWidget::BacksideDefaultChanged, scroll_area, &CardScrollArea::BacksideDefaultChanged);
            QObject::connect(card_options, &CardOptionsWidget::CardBacksideChanged, scroll_area, &CardScrollArea::FullRefresh);
            QObject::connect(global_options, &GlobalOptionsWidget::DisplayColumnsChanged, scroll_area, &CardScrollArea::DisplayColumnsChanged);
            QObject::connect(global_options, &GlobalOptionsWidget::CardOrderChanged, scroll_area, &CardScrollArea::CardOrderChanged);
            QObject::connect(global_options, &GlobalOptionsWidget::CardOrderDirectionChanged, scroll_area, &CardScrollArea::CardOrderDirectionChanged);
        }

        {
            // TODO: Fine-tune these connections to reduce amount of pointless work
            QObject::connect(actions, &ActionsWidget::NewProjectOpened, print_preview, &PrintPreview::Refresh);
            QObject::connect(actions, &ActionsWidget::ImageDirChanged, print_preview, &PrintPreview::Refresh);

            QObject::connect(&card_provider, &CardProvider::CardAdded, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(&card_provider, &CardProvider::CardRemoved, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(&card_provider, &CardProvider::CardRenamed, print_preview, &PrintPreview::RequestRefresh);

            QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(print_options, &PrintOptionsWidget::PageSizeChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(print_options, &PrintOptionsWidget::MarginsChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(print_options, &PrintOptionsWidget::CardLayoutChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(print_options, &PrintOptionsWidget::OrientationChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(print_options, &PrintOptionsWidget::FlipOnChanged, print_preview, &PrintPreview::RequestRefresh);

            QObject::connect(guides_options, &GuidesOptionsWidget::ExactGuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::GuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::BacksideGuidesEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::CornerGuidesChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::CrossGuidesChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::ExtendedGuidesChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::GuidesColorChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::GuidesOffsetChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::GuidesLengthChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(guides_options, &GuidesOptionsWidget::GuidesThicknessChanged, print_preview, &PrintPreview::RequestRefresh);

            QObject::connect(card_options, &CardOptionsWidget::BleedChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(card_options, &CardOptionsWidget::SpacingChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(card_options, &CardOptionsWidget::CornersChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(card_options, &CardOptionsWidget::BacksideEnabledChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(card_options, &CardOptionsWidget::BacksideDefaultChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(card_options, &CardOptionsWidget::BacksideOffsetChanged, print_preview, &PrintPreview::RequestRefresh);

            QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, print_preview, &PrintPreview::RequestRefresh);
            QObject::connect(global_options, &GlobalOptionsWidget::CardOrderChanged, print_preview, &PrintPreview::CardOrderChanged);
            QObject::connect(global_options, &GlobalOptionsWidget::CardOrderDirectionChanged, print_preview, &PrintPreview::CardOrderDirectionChanged);
        }

        {
            QObject::connect(global_options, &GlobalOptionsWidget::RenderBackendChanged, actions, &ActionsWidget::RenderBackendChanged);
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
            QObject::connect(print_options, &PrintOptionsWidget::PageSizesChanged, global_options, &GlobalOptionsWidget::PageSizesChanged);
            QObject::connect(print_options, &PrintOptionsWidget::CardSizesChanged, global_options, &GlobalOptionsWidget::CardSizesChanged);
        }

        {
            QObject::connect(global_options, &GlobalOptionsWidget::PluginEnabled, options_area, &OptionsAreaWidget::PluginEnabled);
            QObject::connect(global_options, &GlobalOptionsWidget::PluginDisabled, options_area, &OptionsAreaWidget::PluginDisabled);
        }

        {
            QObject::connect(global_options, &GlobalOptionsWidget::AdvancedModeChanged, print_options, &PrintOptionsWidget::AdvancedModeChanged);
            QObject::connect(global_options, &GlobalOptionsWidget::AdvancedModeChanged, guides_options, &GuidesOptionsWidget::AdvancedModeChanged);
            QObject::connect(global_options, &GlobalOptionsWidget::AdvancedModeChanged, card_options, &CardOptionsWidget::AdvancedModeChanged);
        }

        {
            QObject::connect(global_options, &GlobalOptionsWidget::MaxWorkerThreadsChanged, main_window, apply_max_worker_threads);
        }

        {
            QObject::connect(
                print_preview,
                &PrintPreview::RestoreCardsOrder,
                &project,
                [&]()
                {
                    project.RestoreCardsOrder();
                    print_preview->Refresh();
                },
                Qt::ConnectionType::QueuedConnection);
            QObject::connect(
                print_preview,
                &PrintPreview::ReorderCards,
                &project,
                [&](size_t from, size_t to)
                {
                    project.ReorderCards(from, to);
                    print_preview->Refresh();
                },
                Qt::ConnectionType::QueuedConnection);
        }

        app.SetMainWindow(main_window);
        main_window->show();

        // Enable and disable Render button
        QObject::connect(&cropper, &Cropper::CropWorkStart, actions, &ActionsWidget::CropperWorking);
        QObject::connect(&cropper, &Cropper::CropWorkDone, actions, &ActionsWidget::CropperDone);
        QObject::connect(&cropper, &Cropper::CropProgress, actions, &ActionsWidget::CropperProgress);

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

        if (g_Cfg.m_CheckVersionOnStartup)
        {
            if (auto new_version{ NewAvailableVersion() })
            {
                main_window->Toast(
                    ToastType::Info,
                    "New version available",
                    QString{ "<a style=\"color:CornflowerBlue\" href=\"%1\">"
                             "Download the new version %2 from GitHub"
                             "</a>" }
                        .arg(ReleaseURL(new_version.value()).c_str())
                        .arg(new_version.value().c_str()));
            }
        }
    }

    // Write preview to project and forward to widgets
    QObject::connect(&cropper, &Cropper::PreviewUpdated, &project, &Project::SetPreview);

    // Write preview cache to file
    QObject::connect(&cropper, &Cropper::PreviewWorkDone, &project, &Project::CropperDone);

    cropper.Start();
    card_provider.Start();

    if (cli.m_NoUI)
    {
        if (cli.m_WaitForCropper)
        {
            QEventLoop evt_loop;
            QObject::connect(&cropper,
                             &Cropper::CropWorkDone,
                             &evt_loop,
                             &QEventLoop::quit);
            evt_loop.exec();

            LogInfo("Cropper finished...");
        }

        if (cli.m_Render)
        {
            GeneratePdf(project);
            LogInfo("PDF rendered...");
        }

        LogInfo("Exiting...");

        return 0;
    }
    else
    {
        const int return_code{ QApplication::exec() };
        project.Dump(app.GetProjectPath());
        return return_code;
    }
}
