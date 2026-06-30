#include <ranges>
#include <string_view>

#include <QCommandLineParser>
#include <QProcess>

#include <magic_enum/magic_enum.hpp>

#ifdef Q_OS_WIN
#define NO_MIN_MAX
#include <Windows.h>
#endif

#include <ppp/auto_update.hpp>
#include <ppp/lib.hpp>

enum ReturnCode : int
{
    RETURN_SUCCESS = 0,
    RETURN_ERROR_LOADING_LIBRARY,
    RETURN_ERROR_INITIALIZING_LIBRARY,
    RETURN_ERROR_EXECUTING_LIBRARY,
    RETURN_ERROR_SHUTTING_DOWN_LIBRARY,
    RETURN_ERROR_UPDATING,

    RETURN_COUNT,
};

void Reboot(std::span<char*> argv, QStringList extra_args = {})
{
    for (auto* arg : argv | std::views::drop(1))
    {
        extra_args.append(arg);
    }
    QProcess::startDetached(argv[0], extra_args);
}

int main(int argc, char** argv)
{
    const std::span args{ argv, static_cast<size_t>(argc) };

    {
        QCommandLineParser cli;

        QCommandLineOption explain_error{
            QStringList{} << "e" << "explain-error",
            "Explain the error code from another execution.",
            "error"
        };
        cli.addOption(explain_error);

        cli.process(QStringList{ args.begin(), args.end() });

        if (cli.isSet(explain_error))
        {
#ifdef Q_OS_WIN
            if (AttachConsole(ATTACH_PARENT_PROCESS))
            {
                freopen("CONOUT$", "w", stdout);
                freopen("CONOUT$", "w", stderr);
                freopen("CONIN$", "r", stdin);
                std::ios::sync_with_stdio();
            }
#endif

            const auto error{ cli.value(explain_error).toInt() };
            const auto error_str{ magic_enum::enum_name(static_cast<ReturnCode>(error)) };
            std::printf("%.*s\n", static_cast<int>(error_str.size()), error_str.data());
            std::fflush(stdout);
            return 0;
        }
    }

    if (auto auto_update_phase{ ResolveAutoUpdatePhase(args) })
    {
        if (auto_update_phase(args))
        {
            return 0;
        }
    }
    else
    {
        switch (AutoUpdateTryInitialize(args))
        {
        case AutoUpdateConclusion::Initiated:
            return 0;
        case AutoUpdateConclusion::Error:
            return RETURN_ERROR_UPDATING;
        default:
            break;
        }
    }

    AppLibrary app_lib{};
    if (!app_lib.Load())
    {
        return RETURN_ERROR_LOADING_LIBRARY;
    }

    if (!app_lib.Initialize(argc, argv))
    {
        return RETURN_ERROR_INITIALIZING_LIBRARY;
    }

    if (!app_lib.Execute())
    {
        return RETURN_ERROR_EXECUTING_LIBRARY;
    }

    const bool reboot_requested{ app_lib.RebootRequested() };

    if (!app_lib.Shutdown())
    {
        return RETURN_ERROR_SHUTTING_DOWN_LIBRARY;
    }

    const int return_code{ app_lib.ReturnCode() + RETURN_COUNT };
    if (reboot_requested)
    {
        Reboot(args);
    }
    return return_code;
}
