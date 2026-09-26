#include <ppp/util/terminate_handler.hpp>

#include <csignal>
#include <cstdlib>
#include <exception>

#include <ppp/util/log.hpp>

void TerminateHandler()
{
    try
    {
        if (const auto current_exception{ std::current_exception() })
        {
            try
            {
                std::rethrow_exception(current_exception);
            }
            catch (const std::exception& e)
            {
                LogError("Unhandled exception: {}", e.what());
            }
            catch (...)
            {
                LogError("Unhandled exception: <unknown exception>");
            }
        }
        else
        {
            LogError("Terminate called!");
        }
    }
    catch (const std::exception& e)
    {
        LogError("Logger failure during terminate: {}", e.what());
    }
    catch (...)
    {
        LogError("Logger failure during terminate: <unknown exception>");
    }

    std::abort();
}

void SegFaultHandler(int /* signal */)
{
    TerminateHandler();
}

void RegisterTerminateHandler()
{
    std::set_terminate(&TerminateHandler);
    std::signal(SIGSEGV, &SegFaultHandler);
}
