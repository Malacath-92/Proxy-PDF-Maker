#include <fmt/format.h>

#include <QApplication>
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

#include <fmt/chrono.h>

#include <nlohmann/json.hpp>

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
#include <ppp/ui/widget_card_area.hpp>
#include <ppp/ui/widget_card_options.hpp>
#include <ppp/ui/widget_global_options.hpp>
#include <ppp/ui/widget_guides_options.hpp>
#include <ppp/ui/widget_options_area.hpp>
#include <ppp/ui/widget_print_options.hpp>
#include <ppp/ui/widget_print_preview.hpp>

#include <ppp/plugins/plugin_interface.hpp>

#include <ppp/profile/profile.hpp>

class PluginRouter : public PluginInterface
{
    virtual QWidget* Widget() override
    {
        return nullptr;
    }
};

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

    {
        // Backwards compatiblity of saving card/page size default in Config
        if (g_Cfg.m_DefaultCardSize.value_or("Standard") != "Standard")
        {
            app.SetProjectDefault("card_size", g_Cfg.m_DefaultCardSize.value());
        }
        if (g_Cfg.m_DefaultPageSize.value_or("Letter") != "Letter")
        {
            app.SetProjectDefault("page_size", g_Cfg.m_DefaultPageSize.value());
        }
    }

    SaveConfig(g_Cfg);

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

    Project project{};
    project.Load(app.GetProjectPath());

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

    auto* card_area{ new CardArea{ project } };
    auto* print_preview{ new PrintPreview{ project } };
    auto* tabs{ new MainTabs{ card_area, print_preview } };

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
    QObject::connect(&plugin_router, &PluginRouter::RefreshCardGrid, card_area, &CardArea::FullRefresh);

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
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, main_window, &PrintProxyPrepMainWindow::ProjectPathChanged);
    }

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, &project, &Project::EnsureOutputFolder);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, &project, &Project::EnsureOutputFolder);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, &project, &Project::EnsureOutputFolder);
        QObject::connect(card_options, &CardOptionsWidget::EnvelopeBleedChanged, &project, &Project::EnsureOutputFolder);
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
        QObject::connect(card_options, &CardOptionsWidget::EnvelopeBleedChanged, &card_provider, &CardProvider::BleedChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::ColorCubeChanged, &card_provider, &CardProvider::ColorCubeChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BasePreviewWidthChanged, &card_provider, &CardProvider::BasePreviewWidthChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::MaxDPIChanged, &card_provider, &CardProvider::MaxDPIChanged);
    }

    {
        QObject::connect(&card_provider, &CardProvider::CardAdded, card_area, &CardArea::CardAdded);
        QObject::connect(&card_provider, &CardProvider::CardRemoved, card_area, &CardArea::CardRemoved);
        QObject::connect(&card_provider, &CardProvider::CardRenamed, card_area, &CardArea::CardRenamed);

        QObject::connect(&project, &Project::CardVisibilityChanged, card_area, &CardArea::CardVisibilityChanged);

        QObject::connect(actions, &ActionsWidget::NewProjectOpened, card_area, &CardArea::NewProjectOpened);
        QObject::connect(actions, &ActionsWidget::ImageDirChanged, card_area, &CardArea::ImageDirChanged);
        QObject::connect(card_options, &CardOptionsWidget::BacksideEnabledChanged, card_area, &CardArea::BacksideEnabledChanged);
        QObject::connect(card_options, &CardOptionsWidget::BacksideDefaultChanged, card_area, &CardArea::BacksideDefaultChanged);
        QObject::connect(card_options, &CardOptionsWidget::CardBacksideChanged, card_area, &CardArea::FullRefresh);
        QObject::connect(global_options, &GlobalOptionsWidget::DisplayColumnsChanged, card_area, &CardArea::DisplayColumnsChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::CardOrderChanged, card_area, &CardArea::CardOrderChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::CardOrderDirectionChanged, card_area, &CardArea::CardOrderDirectionChanged);
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
        QObject::connect(card_options, &CardOptionsWidget::EnvelopeBleedChanged, print_preview, &PrintPreview::RequestRefresh);
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
        QObject::connect(card_options, &CardOptionsWidget::EnvelopeBleedChanged, print_options, &PrintOptionsWidget::BleedChanged);
        QObject::connect(card_options, &CardOptionsWidget::SpacingChanged, print_options, &PrintOptionsWidget::SpacingChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::BaseUnitChanged, print_options, &PrintOptionsWidget::BaseUnitChanged);
        QObject::connect(global_options, &GlobalOptionsWidget::RenderBackendChanged, print_options, &PrintOptionsWidget::RenderBackendChanged);
    }

    {
        QObject::connect(actions, &ActionsWidget::NewProjectOpened, guides_options, &GuidesOptionsWidget::NewProjectOpened);
        QObject::connect(print_options, &PrintOptionsWidget::CardSizeChanged, guides_options, &GuidesOptionsWidget::CardSizeChanged);
        QObject::connect(card_options, &CardOptionsWidget::BleedChanged, guides_options, &GuidesOptionsWidget::BleedChanged);
        QObject::connect(card_options, &CardOptionsWidget::EnvelopeBleedChanged, guides_options, &GuidesOptionsWidget::BleedChanged);
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
        // Move user resources into the right folders
        QObject::connect(main_window, &PrintProxyPrepMainWindow::PdfDropped, &project, [](const auto& path)
                         { fs::copy(path, "res/base_pdfs", fs::copy_options::overwrite_existing); });
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeDropped, &project, [](const auto& path)
                         { fs::copy(path, "res/cubes", fs::copy_options::overwrite_existing); });
        QObject::connect(main_window, &PrintProxyPrepMainWindow::StyleDropped, &project, [](const auto& path)
                         { fs::copy(path, "res/styles", fs::copy_options::overwrite_existing); });
        QObject::connect(main_window, &PrintProxyPrepMainWindow::SvgDropped, &project, [](const auto& path)
                         {
                             fs::copy(path, "res/card_svgs", fs::copy_options::overwrite_existing);
                             
                             // Add a new card size
                             g_Cfg.SvgCardSizeAdded("res/card_svgs" / path.filename()); });

        // Refresh corresponding widgets
        QObject::connect(main_window, &PrintProxyPrepMainWindow::PdfDropped, print_options, &PrintOptionsWidget::BasePdfAdded);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeDropped, global_options, &GlobalOptionsWidget::ColorCubeAdded);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::StyleDropped, global_options, &GlobalOptionsWidget::StyleAdded);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::SvgDropped, print_options, &PrintOptionsWidget::ExternalCardSizesChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::SvgDropped, print_options, &PrintOptionsWidget::CardSizesChanged);
    }

    {
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

    {
        auto apply_max_worker_threads{
            []()
            {
                QThreadPool::globalInstance()->setMaxThreadCount(g_Cfg.m_MaxWorkerThreads);
            }
        };
        apply_max_worker_threads();
        QObject::connect(global_options, &GlobalOptionsWidget::MaxWorkerThreadsChanged, main_window, apply_max_worker_threads);
    }

    cropper.Start();
    card_provider.Start();

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

    TRACY_NAMED_SCOPE(main_run);

    const int return_code{ QApplication::exec() };
    project.Dump(app.GetProjectPath());
    return return_code;
}
