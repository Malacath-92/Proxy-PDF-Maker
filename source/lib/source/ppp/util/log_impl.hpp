#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <ppp/util/log.hpp>

class Log::LogImpl
{
  public:
    LogImpl(LogFlags log_flags, std::string_view log_name);

    ~LogImpl();

    static Log* GetInstance(std::string_view log_name);

    void RegisterInstance(Log* parent_log);
    void UnregisterInstance();

    static bool RegisterThreadName(std::string_view thread_name);
    static std::string_view GetThreadName(const std::thread::id& thread_id);

    uint32_t InstallHook(typename Log::LogHook hook);
    void UninstallHook(uint32_t hook_id);

    bool GetStacktraceEnabled(Log::LogLevel level) const;

    void Print(const Log::DetailInformation& detail_info, Log::LogLevel level, const char* message);

  private:
    void Flush(const DetailInformation& detail_info, LogLevel level, const char* message);

    void CreateLogFile();

    Log* m_ParentLog{ nullptr };

    const std::string m_LogName;
    std::mutex m_Mutex;

    struct InstalledLogHook
    {
        uint32_t m_HookId;
        typename Log::LogHook m_Hook;
    };
    std::vector<InstalledLogHook> m_LogHooks;

    std::ofstream m_FileStream;

    const LogFlags m_LogFlags;

    inline static std::shared_mutex g_InstanceListMutex;
    inline static std::unordered_map<std::string, LogImpl*> g_Instances;

    inline static std::shared_mutex g_ThreadListMutex;
    inline static std::unordered_map<std::thread::id, std::string> g_ThreadList;
};
