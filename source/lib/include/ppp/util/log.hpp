#pragma once

#include <ctime>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#ifdef __cpp_lib_stacktrace
#include <stacktrace>
#endif

#include <fmt/format.h>

#include <ppp/util/type_traits.hpp>
#include <ppp/util/typedefs.hpp>

/*
        A Log can be called from any thread and will write to the console and/or a specified file
        Both are optional and choosen by the client at initialization
*/
class Log
{
  public:
    Log(LogFlags log_flags, std::string_view log_name);
    ~Log();

    /*
            Getter for all instances of a logger
    */
    static Log* GetInstance(std::string_view log_name);

    /*
            Register a thread's name
    */
    static bool RegisterThreadName(std::string_view thread_name);
    static std::string_view GetThreadName(const std::thread::id& thread_id);

    /*
            LogLevels dictate the severity of the log
            This is clearly intended for developers and not for users
    */
    enum class LogLevel
    {
        Information,
        Debug,
        Warning,
        Error,
        Fatal
    };

    /*
            The Detail information that can be used to give a developer more information about a log
    */
    struct DetailInformation
    {
        std::time_t Time;
        std::string_view File;
        std::size_t Line;
        std::size_t Column;
        std::string_view Function;
        std::string_view Thread;
        std::vector<std::string> StackTrace;
    };

    /*
            Register a hook for external handling of messages
    */
    using LogHook = std::function<void(const Log::DetailInformation&, Log::LogLevel, std::string_view)>;
    uint32_t InstallHook(LogHook hook);
    void UninstallHook(uint32_t hook_id);

    /*
            Wrapper for a log message, ensures that used strings are constant expressions
    */
    template<class... Args>
    struct LogMessageWrapper
    {
        consteval LogMessageWrapper(const char* message, std::source_location source_info = std::source_location::current())
            : Message{ message }
            , SourceInfo{ source_info }
        {
        }

        fmt::format_string<Args...> Message{};
        std::source_location SourceInfo{};
    };
    template<class... Args>
    using LogMessage = LogMessageWrapper<identity_t<Args>...>;

    /*
            For the sake of optimization and avoiding stack trace generation when not needed
    */
    bool GetStacktraceEnabled(LogLevel level) const;

    /*
            Raw Print function accepts a null-terminated string, otherwise it causes undefined behaviour
            Templated print function uses fmtlib for formatting
    */
    void PrintRaw(const DetailInformation& detail_info, LogLevel level, const char* message);
    template<std::size_t BufferSize = 256, class... Args>
    void Print(const DetailInformation& detail_info, LogLevel level, fmt::format_string<Args...> message, Args&&... args)
    {
        if constexpr (BufferSize == 0)
        {
            const auto formatted = fmt::format(message, std::forward<Args>(args)...);
            PrintRaw(detail_info, level, formatted.c_str());
        }
        else
        {
            char buffer[BufferSize]{};
            const auto format_result = fmt::format_to_n(buffer, BufferSize - 1, message, std::forward<Args>(args)...);
            const auto required = format_result.size + 1;
            if (required > BufferSize)
            {
                constexpr const char buffer_size_error[] = R"(
Call to Log::Print<BufferSize> reqiures a bigger buffer here, called with {} but requires at least {}.
)";
                Print<256>(detail_info, level, buffer_size_error, BufferSize, required);
            }
            PrintRaw(detail_info, level, (const char*)buffer);
        }
    }

    template<std::size_t BufferSize, class... Args>
    static void DoLog(std::string_view log_name, LogLevel level, const LogMessage<Args...>& message, Args&&... args)
    {
        Log* log_sink = Log::GetInstance(log_name);
        if (log_sink)
        {
#ifdef _WIN32
            std::string file{ message.SourceInfo.file_name() };
            std::ranges::replace(file, '\\', '/');
#else
            std::string_view file{ message.SourceInfo.file_name() };
#endif
            DetailInformation detail_info{
                std::time(nullptr),
                file,
                message.SourceInfo.line(),
                message.SourceInfo.column(),
                message.SourceInfo.function_name(),
                GetThreadName(std::this_thread::get_id()),
                {},
            };

#ifdef __cpp_lib_stacktrace
            if (log_sink->GetStacktraceEnabled(level))
            {
                std::stacktrace stacktrace = std::stacktrace::current(2);
                for (const auto& stack_elem : stacktrace)
                {
                    const std::string source_file = stack_elem.source_file();
                    if (!source_file.empty())
                    {
                        detail_info.StackTrace.push_back(
                            fmt::format("  {:<64} @ {}:{}",
                                        stack_elem.description(),
                                        source_file,
                                        stack_elem.source_line()));
                    }
                    else
                    {
                        detail_info.StackTrace.push_back(
                            fmt::format("  {}", stack_elem.description()));
                    }
                }

                if (!detail_info.StackTrace.empty())
                {
                    detail_info.StackTrace[0][0] = '>';
                }
            }
#endif
            log_sink->Print<BufferSize>(detail_info, level, message.Message, std::forward<Args>(args)...);
        }
    }

    /*
            The name of the main log, globally available so one can query for the main log or create it themselves
    */
    static constexpr std::string_view m_MainLogName{ "Main-Log" };

  private:
    class LogImpl;
    std::unique_ptr<LogImpl> mImpl;
};

template<std::size_t BufferSize, class... Args>
void LogInfoEx(std::string_view log_name, const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<BufferSize>(log_name, Log::LogLevel::Information, message, std::forward<Args>(args)...);
}
template<std::size_t BufferSize, class... Args>
void LogDebugEx(std::string_view log_name, const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<BufferSize>(log_name, Log::LogLevel::Debug, message, std::forward<Args>(args)...);
}
template<std::size_t BufferSize, class... Args>
void LogWarningEx(std::string_view log_name, const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<BufferSize>(log_name, Log::LogLevel::Warning, message, std::forward<Args>(args)...);
}
template<std::size_t BufferSize, class... Args>
void LogErrorEx(std::string_view log_name, const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<BufferSize>(log_name, Log::LogLevel::Error, message, std::forward<Args>(args)...);
}
template<std::size_t BufferSize, class... Args>
void LogFatalEx(std::string_view log_name, const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<BufferSize>(log_name, Log::LogLevel::Fatal, message, std::forward<Args>(args)...);
}

template<class... Args>
void LogInfo(const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<0>(::Log::m_MainLogName, Log::LogLevel::Information, message, std::forward<Args>(args)...);
}
template<class... Args>
void LogDebug(const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<0>(::Log::m_MainLogName, Log::LogLevel::Debug, message, std::forward<Args>(args)...);
}
template<class... Args>
void LogWarning(const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<0>(::Log::m_MainLogName, Log::LogLevel::Warning, message, std::forward<Args>(args)...);
}
template<class... Args>
void LogError(const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<0>(::Log::m_MainLogName, Log::LogLevel::Error, message, std::forward<Args>(args)...);
}
template<class... Args>
void LogFatal(const Log::LogMessage<Args...>& message, Args&&... args)
{
    Log::DoLog<0>(::Log::m_MainLogName, Log::LogLevel::Fatal, message, std::forward<Args>(args)...);
}
