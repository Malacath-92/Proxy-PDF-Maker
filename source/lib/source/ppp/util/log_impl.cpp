#include <ppp/util/log_impl.hpp>

#ifdef __cpp_lib_stacktrace
#include <stacktrace>
#endif
#include <filesystem>
#include <sstream>

#include <QDebug>

#include <fmt/chrono.h>

Log::LogImpl::LogImpl(LogFlags log_flags, std::string_view log_name)
    : m_LogName(log_name)
    , m_LogFlags(log_flags)
{
    LogDebug("Constructing log sink '{}'!", m_LogName);

    if (bool(m_LogFlags & LogFlags::File))
        CreateLogFile();
}

Log::LogImpl::~LogImpl()
{
    LogDebug("Destroying log sink '{}'!", m_LogName);

    UnregisterInstance();
}

Log* Log::LogImpl::GetInstance(std::string_view log_name)
{
    std::shared_lock<std::shared_mutex> write_lock(g_InstanceListMutex);

    // TODO: C++20 heterogenous lookup
    // auto it = g_Instances.find(log_name);

    auto it = g_Instances.find(std::string{ log_name });
    if (it != g_Instances.end())
        return it->second->m_ParentLog;
    return nullptr;
}

void Log::LogImpl::RegisterInstance(Log* parent_log)
{
    UnregisterInstance();

    m_ParentLog = parent_log;

    std::unique_lock<std::shared_mutex> write_lock(g_InstanceListMutex);
    if (g_Instances.find(m_LogName) != g_Instances.end())
        throw "Log-Name Redefinition";
    g_Instances[m_LogName] = this;
}
void Log::LogImpl::UnregisterInstance()
{
    m_ParentLog = nullptr;

    if (g_Instances.count(m_LogName) > 0)
    {
        std::unique_lock<std::shared_mutex> write_lock(g_InstanceListMutex);
        g_Instances.erase(m_LogName);
    }
}

bool Log::LogImpl::RegisterThreadName(std::string_view thread_name)
{
    std::unique_lock<std::shared_mutex> write_lock(g_ThreadListMutex);

    std::thread::id thread_id = std::this_thread::get_id();
    auto it = g_ThreadList.find(thread_id);
    if (it != g_ThreadList.end())
        return false;

    g_ThreadList[thread_id] = thread_name;
    return true;
}

std::string_view Log::LogImpl::GetThreadName(const std::thread::id& thread_id)
{
    std::shared_lock<std::shared_mutex> read_lock(g_ThreadListMutex);

    auto it = g_ThreadList.find(thread_id);
    if (it != g_ThreadList.end())
        return it->second;

    return "Unregistered";
}

uint32_t Log::LogImpl::InstallHook(Log::LogHook hook)
{
    std::lock_guard lock{ m_Mutex };
    m_LogHooks.push_back({ static_cast<uint32_t>(m_LogHooks.size() + 1), std::move(hook) });
    return m_LogHooks.back().m_HookId;
}

void Log::LogImpl::UninstallHook(uint32_t hook_id)
{
    std::lock_guard lock{ m_Mutex };
    for (auto it = m_LogHooks.begin(); it != m_LogHooks.end(); ++it)
    {
        if (it->m_HookId == hook_id)
        {
            m_LogHooks.erase(it);
            return;
        }
    }
}

bool Log::LogImpl::GetStacktraceEnabled(LogLevel level) const
{
    LogFlags stacktrace_bits = m_LogFlags & LogFlags::DetailStacktrace;
    return (level == LogLevel::Error && bool(stacktrace_bits & LogFlags::DetailErrorStacktrace)) || (level == LogLevel::Fatal && bool(stacktrace_bits & LogFlags::DetailFatalStacktrace));
}

void Log::LogImpl::Print(const Log::DetailInformation& detail_info, Log::LogLevel level, const char* message)
{
    Flush(detail_info, level, message);

    if (bool(m_LogFlags & LogFlags::FatalQuit) && level == LogLevel::Fatal)
    {
        exit(-1);
    }
}

void Log::LogImpl::Flush(const DetailInformation& detail_info, LogLevel level, const char* message)
{
    std::stringstream stream;

    if (message[0] == '\n')
    {
        stream << "\n";
        message++;
    }

    // The error prefix
    const char* prefix{ "[???]" };
    switch (level)
    {
    case LogLevel::Information:
        prefix = " [INFO]";
        break;
    case LogLevel::Debug:
        prefix = "[DEBUG]";
        break;
    case LogLevel::Warning:
        prefix = " [WARN]";
        break;
    case LogLevel::Error:
        prefix = "[ERROR]";
        break;
    case LogLevel::Fatal:
        prefix = "[FATAL]";
        break;
    }
    stream << prefix;

    // Detailed information, e.g. time, file, line...
    LogFlags detail_bits = m_LogFlags & LogFlags::DetailAll;
    if (bool(detail_bits))
    {
        const auto get_highest_order_bit{
            [](LogFlags flags)
            {
                using underlying_t = std::underlying_type_t<LogFlags>;
                underlying_t cast_flags = static_cast<underlying_t>(flags);
                underlying_t max = 0x0;

                for (size_t i = 1; i < sizeof(underlying_t) * 8; i++)
                {
                    max = std::max(max, (0x1 << i) & cast_flags);
                }

                return static_cast<LogFlags>(max);
            }
        };
        LogFlags highest_bit = get_highest_order_bit(detail_bits);

        // Conditional delimiter
#define CON_DEL(bit) (bool(bit & highest_bit) ? "" : "; ")

        stream << "<";
        if (bool(detail_bits & LogFlags::DetailTime))
            stream << detail_info.m_Time << CON_DEL(LogFlags::DetailTime);
        if (bool(detail_bits & LogFlags::DetailFile))
        {
            if (detail_info.m_File.starts_with(PPP_SOURCE_ROOT))
            {
                stream << detail_info.m_File.substr(strlen(PPP_SOURCE_ROOT)) << CON_DEL(LogFlags::DetailFile);
            }
            else
            {
                stream << detail_info.m_File << CON_DEL(LogFlags::DetailFile);
            }
        }
        if (bool(detail_bits & LogFlags::DetailLine))
        {
            if (bool(detail_bits & LogFlags::DetailColumn))
            {
                stream << detail_info.m_Line << ":" << detail_info.m_Column << CON_DEL(LogFlags::DetailColumn);
            }
            else
            {
                stream << detail_info.m_Line << CON_DEL(LogFlags::DetailLine);
            }
        }
        if (bool(detail_bits & LogFlags::DetailFunction))
            stream << detail_info.m_Function << CON_DEL(LogFlags::DetailFunction);
        if (bool(detail_bits & LogFlags::DetailThread))
            stream << detail_info.m_Thread << CON_DEL(LogFlags::DetailThread);
        stream << ">";

#undef CON_DEL
    }

    stream << ": ";

    // The message
    stream << std::string_view(message) << "\n";

    if (GetStacktraceEnabled(level))
    {
#ifdef __cpp_lib_stacktrace
        if (!detail_info.m_StackTrace.empty())
        {
            stream << "Stacktrace:\n";
            for (std::string_view stack_element : detail_info.m_StackTrace)
            {
                stream << stack_element << "\n";
            }
        }
        else
#endif
        {
            stream << "[[Stacktrace not available]]\n";
        }
    }

    const std::string full_message_str{ stream.str() };
    std::lock_guard lock{ m_Mutex };

    // Print the log
    if (bool(m_LogFlags & LogFlags::Console))
        qDebug() << full_message_str;
    if (m_FileStream.is_open())
        m_FileStream << full_message_str;

    // Flush the streams
    if (m_FileStream.is_open())
        m_FileStream << std::flush;

    // Forward to hooks
    for (const InstalledLogHook& hook : m_LogHooks)
    {
        hook.m_Hook(detail_info, level, message);
    }
}

void Log::LogImpl::CreateLogFile()
{
    namespace fs = std::filesystem;

    char file_name_buffer[256]{};
    fmt::format_to_n(file_name_buffer, 255, "logs/{:%Y-%m-%d_%H-%M-%S}.log", fmt::localtime(std::time(nullptr)));

    fs::path logs_directory{ fs::absolute("logs") };
    if (!fs::exists(logs_directory) || !fs::is_directory(logs_directory))
    {
        if (fs::exists(logs_directory))
        {
            fs::remove_all(logs_directory);
        }
        fs::create_directories(logs_directory);
    }

    std::multimap<fs::file_time_type, fs::path> files_sorted_by_modify_time;
    for (fs::directory_iterator dir_iter(logs_directory); dir_iter != fs::directory_iterator{}; ++dir_iter)
    {
        if (fs::is_regular_file(dir_iter->status()))
        {
            files_sorted_by_modify_time.insert({ fs::last_write_time(dir_iter->path()), dir_iter->path() });
        }
    }

    static constexpr std::size_t c_MaxNumLogFiles = 256;
    const std::size_t num_files = files_sorted_by_modify_time.size();
    if (num_files > c_MaxNumLogFiles)
    {
        const std::size_t files_to_delete = num_files - c_MaxNumLogFiles;
        for (std::size_t i = 0; i < files_to_delete; i++)
        {
            auto it = files_sorted_by_modify_time.begin();
            std::advance(it, i);
            fs::remove(it->second);
        }
    }

    std::lock_guard lock{ m_Mutex };
    m_FileStream.open(file_name_buffer);
}
