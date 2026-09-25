#include <ppp/update_flow.hpp>

#include <QApplication>
#include <QDesktopServices>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/ui/main_window.hpp>

#include <ppp/auto_update.hpp>
#include <ppp/version_check.hpp>

#include <ppp/profile/profile.hpp>

static constexpr char g_AutoUpdate[]{ "#auto-update" };

void Reboot()
{
    QProcess::startDetached(
        QApplication::arguments()[0],
        QApplication::arguments().mid(1));
    QApplication::exit();
}

void DoAutoUpdate(PrintProxyPrepMainWindow* main_window,
                  std::string_view version)
{
    class DownloadToastHandler : public ToastHandler
    {
      public:
        virtual bool hasDynamicText() const
        {
            return true;
        }

        virtual bool hasOnLink() const
        {
            return false;
        }
        virtual bool onLink(const QString& /* link */)
        {
            return false;
        }

        virtual bool hasProgress() const
        {
            return true;
        }
    };

    DownloadToastHandler toast_handler;
    ToastData download_toast{
        .m_Type = ToastType::Info,
        .m_Title{ "Downloading new version" },
        .m_Message{ "Download progress..." },
        .m_Handler{ &toast_handler },
        .m_HandlerExternallyOwned{ true },
    };
    main_window->Toast(download_toast);

    const auto download_progress_fn{
        [&toast_handler](std::string_view work_title, float progress)
        {
            toast_handler.textChanged(ToQString(work_title) + "...");
            toast_handler.progress(progress / 100.0f);
        }
    };
    if (AutoUpdateDownloadRelease(version, download_progress_fn))
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
                return true;
            });
    }
}

void RunUpdateFlow(PrintProxyPrepMainWindow* main_window)
{
    TRACY_AUTO_SCOPE();

    if (auto new_version{ NewAvailableVersion() })
    {
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
                .arg(g_AutoUpdate),
            [new_version, main_window](const QString& link)
            {
                if (link == g_AutoUpdate)
                {
                    auto do_auto_update{
                        [main_window, new_version]()
                        {
                            auto* main_log{ Log::GetInstance(Log::c_MainLogName) };
                            const auto log_hook{
                                main_log->InstallTemporaryHook(
                                    [&](const Log::DetailInformation&, Log::LogLevel log_level, std::string_view message)
                                    {
                                        if (log_level == Log::LogLevel::Error)
                                        {
                                            main_window->Toast(ToastType::Error,
                                                               "Auto-Update Error",
                                                               QString{ "Failed downloading new version: %1" }.arg(ToQString(message)));
                                        }
                                    })
                            };

                            try
                            {
                                DoAutoUpdate(main_window, new_version.value());
                            }
                            catch (const std::exception& e)
                            {
                                LogError("Exception '{}' thrown", e.what());
                            }
                        }
                    };
                    QTimer::singleShot(
                        0,
                        do_auto_update);
                }
                else
                {
                    QDesktopServices::openUrl(link);
                }

                return true;
            });
    }
}
