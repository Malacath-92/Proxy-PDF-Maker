#include <ppp/terminate_handler.hpp>

#include <exception>

#include <ppp/util/log.hpp>

void CppTraceTerminateHandler()
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

void RegisterTerminateHandler()
{
    std::set_terminate(CppTraceTerminateHandler);
}
