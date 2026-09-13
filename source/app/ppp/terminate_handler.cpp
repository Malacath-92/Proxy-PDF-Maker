#include <ppp/terminate_handler.hpp>

#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

#include <ppp/util/log.hpp>

void CppTraceTerminateHandler()
{
    try
    {
        if (const auto current_exception{ std::current_exception() })
        {
            const auto trace{ cpptrace::generate_trace() };

            try
            {
                std::rethrow_exception(current_exception);
            }
            catch (const std::exception& e)
            {
                LogError("Unhandled exception: {}\n{}", e.what(), trace.to_string());
            }
            catch (...)
            {
                LogError("Unhandled exception: <unknown exception>\n{}", trace.to_string());
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
