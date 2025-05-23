#include <ppp/util/log.hpp>

#include <ppp/util/log_impl.hpp>

Log::Log(LogFlags log_flags, std::string_view log_name)
{
    m_Impl = std::make_unique<LogImpl>(log_flags, log_name);
    m_Impl->RegisterInstance(this);
}

Log::~Log() = default;

Log* Log::GetInstance(std::string_view log_name)
{
    return LogImpl::GetInstance(log_name);
}

bool Log::RegisterThreadName(std::string_view thread_name)
{
    return LogImpl::RegisterThreadName(thread_name);
}

std::string_view Log::GetThreadName(const std::thread::id& thread_id)
{
    return LogImpl::GetThreadName(thread_id);
}

uint32_t Log::InstallHook(LogHook hook)
{
    return m_Impl->InstallHook(std::move(hook));
}
void Log::UninstallHook(uint32_t hook_id)
{
    return m_Impl->UninstallHook(hook_id);
}

bool Log::GetStacktraceEnabled(LogLevel level) const
{
    return m_Impl->GetStacktraceEnabled(level);
}

void Log::PrintRaw(const DetailInformation& detail_info, LogLevel level, const char* message)
{
    m_Impl->Print(detail_info, level, message);
}
