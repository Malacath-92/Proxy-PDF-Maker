#include <ppp/util/log_impl.hpp>

#ifdef __cpp_lib_stacktrace
#include <stacktrace>
#endif
#include <sstream>

#include <QDebug>

#include <fmt/chrono.h>

Log::LogImpl::LogImpl(LogFlags log_flags, std::string_view log_name)
    : mLogName(log_name)
    , mLogFlags(log_flags)
{
    LogInfo("Constructing log sink '{}'!", mLogName);

    if (bool(mLogFlags & LogFlags::File))
        CreateLogFile();
}

Log::LogImpl::~LogImpl()
{
    LogInfo("Destroying log sink '{}'!", mLogName);

    UnregisterInstance();
}

Log* Log::LogImpl::GetInstance(std::string_view log_name)
{
    std::shared_lock<std::shared_mutex> writeLock(m_InstanceListMutex);

    // TODO: C++20 heterogenous lookup
    // auto it = m_Instances.find(log_name);

    auto it = m_Instances.find(std::string{ log_name });
    if (it != m_Instances.end())
        return it->second->mParentLog;
    return nullptr;
}

void Log::LogImpl::RegisterInstance(Log* parent_log)
{
    UnregisterInstance();

    mParentLog = parent_log;

    std::unique_lock<std::shared_mutex> writeLock(m_InstanceListMutex);
    if (m_Instances.find(mLogName) != m_Instances.end())
        throw "Log-Name Redefinition";
    m_Instances[mLogName] = this;
}
void Log::LogImpl::UnregisterInstance()
{
    mParentLog = nullptr;

    if (m_Instances.count(mLogName) > 0)
    {
        std::unique_lock<std::shared_mutex> writeLock(m_InstanceListMutex);
        m_Instances.erase(mLogName);
    }
}

bool Log::LogImpl::RegisterThreadName(std::string_view thread_name)
{
    std::unique_lock<std::shared_mutex> writeLock(m_ThreadListMutex);

    std::thread::id thread_id = std::this_thread::get_id();
    auto it = m_ThreadList.find(thread_id);
    if (it != m_ThreadList.end())
        return false;

    m_ThreadList[thread_id] = thread_name;
    return true;
}

std::string_view Log::LogImpl::GetThreadName(const std::thread::id& thread_id)
{
    std::shared_lock<std::shared_mutex> readLock(m_ThreadListMutex);

    auto it = m_ThreadList.find(thread_id);
    if (it != m_ThreadList.end())
        return it->second;

    return "Unregistered";
}

uint32_t Log::LogImpl::InstallHook(Log::LogHook hook)
{
    std::lock_guard lock{ mMutex };
    mLogHooks.push_back({ static_cast<uint32_t>(mLogHooks.size() + 1), std::move(hook) });
    return mLogHooks.back().mHookId;
}

void Log::LogImpl::UninstallHook(uint32_t hook_id)
{
    std::lock_guard lock{ mMutex };
    for (auto it = mLogHooks.begin(); it != mLogHooks.end(); ++it)
    {
        if (it->mHookId == hook_id)
        {
            mLogHooks.erase(it);
            return;
        }
    }
}

bool Log::LogImpl::GetStacktraceEnabled(LogLevel level) const
{
    LogFlags stacktraceBits = mLogFlags & LogFlags::DetailStacktrace;
    return (level == LogLevel::Error && bool(stacktraceBits & LogFlags::DetailErrorStacktrace)) || (level == LogLevel::Fatal && bool(stacktraceBits & LogFlags::DetailFatalStacktrace));
}

void Log::LogImpl::Print(const Log::DetailInformation& detail_info, Log::LogLevel level, const char* message)
{
    Flush(detail_info, level, message);

    if (bool(mLogFlags & LogFlags::FatalQuit) && level == LogLevel::Fatal)
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
    LogFlags detailBits = mLogFlags & LogFlags::DetailAll;
    if (bool(detailBits))
    {
        const auto get_highest_order_bit{
            [](LogFlags flags)
            {
                using underlying_t = std::underlying_type_t<LogFlags>;
                underlying_t castFlags = static_cast<underlying_t>(flags);
                underlying_t max = 0x0;

                for (size_t i = 1; i < sizeof(underlying_t) * 8; i++)
                {
                    max = std::max(max, (0x1 << i) & castFlags);
                }

                return static_cast<LogFlags>(max);
            }
        };
        LogFlags highestBit = get_highest_order_bit(detailBits);

        // Conditional delimiter
#define CON_DEL(bit) (bool(bit & highestBit) ? "" : "; ")

        stream << "<";
        if (bool(detailBits & LogFlags::DetailTime))
            stream << detail_info.Time << CON_DEL(LogFlags::DetailTime);
        if (bool(detailBits & LogFlags::DetailFile))
        {
            if (detail_info.File.starts_with(PPP_SOURCE_ROOT))
            {
                stream << detail_info.File.substr(strlen(PPP_SOURCE_ROOT)) << CON_DEL(LogFlags::DetailFile);
            }
            else
            {
                stream << detail_info.File << CON_DEL(LogFlags::DetailFile);
            }
        }
        if (bool(detailBits & LogFlags::DetailLine))
        {
            if (bool(detailBits & LogFlags::DetailColumn))
            {
                stream << detail_info.Line << ":" << detail_info.Column << CON_DEL(LogFlags::DetailColumn);
            }
            else
            {
                stream << detail_info.Line << CON_DEL(LogFlags::DetailLine);
            }
        }
        if (bool(detailBits & LogFlags::DetailFunction))
            stream << detail_info.Function << CON_DEL(LogFlags::DetailFunction);
        if (bool(detailBits & LogFlags::DetailThread))
            stream << detail_info.Thread << CON_DEL(LogFlags::DetailThread);
        stream << ">";

#undef CON_DEL
    }

    stream << ": ";

    // The message
    stream << std::string_view(message) << "\n";

    if (GetStacktraceEnabled(level))
    {
#ifdef __cpp_lib_stacktrace
        if (!detail_info.StackTrace.empty())
        {
            stream << "Stacktrace:\n";
            for (std::string_view stack_element : detail_info.StackTrace)
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
    std::lock_guard lock{ mMutex };

    // Print the log
    if (bool(mLogFlags & LogFlags::Console))
        qDebug() << full_message_str;
    if (mFileStream.is_open())
        mFileStream << full_message_str;

    // Flush the streams
    if (mFileStream.is_open())
        mFileStream << std::flush;

    // Forward to hooks
    for (const InstalledLogHook& hook : mLogHooks)
    {
        hook.mHook(detail_info, level, message);
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

    std::lock_guard lock{ mMutex };
    mFileStream.open(file_name_buffer);
}
