#pragma once

#define _TRACY_MERGE_(a, b) a##b
#define _TRACY_LABEL_(a) _TRACY_MERGE_(unique_name_, a)
#define _TRACY_UNIQUE_NAME_ _TRACY_LABEL_(__LINE__)

#ifdef PPP_TRACY_PROFILING

#include <functional>
#include <memory>
#include <source_location>

#include <fmt/format.h>

#include <ppp/color.hpp>
#include <ppp/util/at_scope_exit.hpp>

struct TracySourceInfo
{
    const char* m_Name;
    const char* m_Function;
    const char* m_File;
    uint32_t m_Line;
    uint32_t m_Color{ 0x0 };
};

void TracyWaitConnect();

void TracyPushScope(const TracySourceInfo& source_info);
void TracyPopScope();

void TracyScopeName(const char* name);
void TracyScopeName(std::string name);
template<class... ArgsT>
void TracyScopeNameFmt(fmt::format_string<ArgsT...> format, ArgsT&&... args)
{
    TracyScopeName(fmt::format(format, std::forward<ArgsT>(args)...));
}

void TracyScopeInfo(const char* info);
void TracyScopeInfo(std::string info);
template<class... ArgsT>
void TracyScopeInfoFmt(fmt::format_string<ArgsT...> format, ArgsT&&... args)
{
    TracyScopeInfo(fmt::format(format, std::forward<ArgsT>(args)...));
}

void TracyScopeColor(uint32_t color);
void TracyScopeColor(ColorRGB8 color);

class TracyMutexBase
{
  public:
    TracyMutexBase(TracySourceInfo source_info);
    ~TracyMutexBase();

    void pre_lock() const;
    void post_lock() const;
    void post_lock(const TracySourceInfo& source_info) const;

    void post_try_lock(bool acquired) const;
    void post_try_lock(bool acquired, const TracySourceInfo& source_info) const;

    void post_unlock() const;

  private:
    TracySourceInfo m_SourceInfo;
    mutable void* m_Handle{ nullptr };
};

template<class MutexT>
concept IsSharedMutex = requires(MutexT& mutex) {
    mutex.lock_shared();
    mutex.try_lock_shared();
    mutex.unlock_shared();
};

template<class MutexT>
class TracyMutex : public TracyMutexBase
{
  public:
    using TracyMutexBase::TracyMutexBase;

    void lock() const
    {
        pre_lock();
        m_Mutex.lock();
        post_lock();
    }
    void lock(const TracySourceInfo& source_info) const
    {
        pre_lock();
        m_Mutex.lock();
        post_lock(source_info);
    }

    bool try_lock() const
    {
        pre_lock();
        bool acquired{ m_Mutex.try_lock() };
        post_try_lock(acquired);
        return acquired;
    }
    bool try_lock(const TracySourceInfo& source_info) const
    {
        pre_lock();
        bool acquired{ m_Mutex.try_lock() };
        post_try_lock(acquired, source_info);
        return acquired;
    }

    void unlock() const
    {
        m_Mutex.unlock();
        post_unlock();
    }

    auto lock_shared() const
    requires IsSharedMutex<MutexT>
    {
        pre_lock();
        m_Mutex.lock_shared();
        post_lock();
    }
    auto lock_shared(const TracySourceInfo& source_info) const
    requires IsSharedMutex<MutexT>
    {
        pre_lock();
        m_Mutex.lock_shared();
        post_lock(source_info);
    }

    auto try_lock_shared() const
    requires IsSharedMutex<MutexT>
    {
        pre_lock();
        bool acquired{ m_Mutex.try_lock_shared() };
        post_try_lock(acquired);
        return acquired;
    }
    auto try_lock_shared(const TracySourceInfo& source_info) const
    requires IsSharedMutex<MutexT>
    {
        pre_lock();
        bool acquired{ m_Mutex.try_lock_shared() };
        post_try_lock(acquired, source_info);
        return acquired;
    }

    auto unlock_shared() const
    requires IsSharedMutex<MutexT>
    {
        m_Mutex.unlock_shared();
        post_unlock();
    }

  private:
    mutable MutexT m_Mutex;
};

#define TRACY_WAIT_CONNECT() TracyWaitConnect()

#define _TRACY__SOURCE_LOC_LABEL_(a) _TRACY_MERGE_(source_loc_, a)
#define _TRACY_UNIQUE_SOURCE_LOC_NAME_ _TRACY__SOURCE_LOC_LABEL_(__LINE__)

// clang-format off
#define TRACY_SOURCE_INFO_INITIALIZER()              \
    {                                                \
        nullptr,                                     \
        __func__,                                    \
        std::source_location::current().file_name(), \
        std::source_location::current().line(),      \
    }
// clang-format on
#define TRACY_STATIC_SOURCE_INFO()                                  \
    static constexpr TracySourceInfo _TRACY_UNIQUE_SOURCE_LOC_NAME_ \
    TRACY_SOURCE_INFO_INITIALIZER()

#define TRACY_PUSH_SCOPE()      \
    TRACY_STATIC_SOURCE_INFO(); \
    TracyPushScope(_TRACY_UNIQUE_SOURCE_LOC_NAME_)
#define TRACY_POP_SCOPE() TracyPopScope()
#define TRACY_AUTO_SCOPE()          \
    TRACY_PUSH_SCOPE();             \
    AtScopeExit _TRACY_UNIQUE_NAME_ \
    {                               \
        TracyPopScope               \
    }

#define TRACY_SCOPE_NAME(name) TracyScopeName(#name)
#define TRACY_SCOPE_NAME_FMT(fmt, ...) TracyScopeNameFmt(fmt, ##__VA_ARGS__)
#define TRACY_SCOPE_INFO(info) TracyScopeInfo(#info)
#define TRACY_SCOPE_INFO_FMT(fmt, ...) TracyScopeInfoFmt(fmt, ##__VA_ARGS__)
#define TRACY_SCOPE_COLOR(color) TracyScopeColor(color)

#define TRACY_DECLARE_MUTEX(mutex_type, mutex_name)                        \
    TracyMutex<mutex_type> mutex_name                                      \
    {                                                                      \
        []() { return TracySourceInfo TRACY_SOURCE_INFO_INITIALIZER(); }() \
    }
#define TRACY_SCOPED_LOCK(mutex)                                     \
    static_assert(std::is_base_of_v<TracyMutexBase,                  \
                                    std::decay_t<decltype(mutex)>>); \
    TRACY_STATIC_SOURCE_INFO();                                      \
    mutex.lock(_TRACY_UNIQUE_SOURCE_LOC_NAME_);                      \
    AtScopeExit _TRACY_UNIQUE_NAME_                                  \
    {                                                                \
        [mut = (&mutex)]() { mut->unlock(); }                        \
    }
#define TRACY_SCOPED_SHARED_LOCK(mutex)                              \
    static_assert(std::is_base_of_v<TracyMutexBase,                  \
                                    std::decay_t<decltype(mutex)>>); \
    TRACY_STATIC_SOURCE_INFO();                                      \
    mutex.lock_shared(_TRACY_UNIQUE_SOURCE_LOC_NAME_);               \
    AtScopeExit _TRACY_UNIQUE_NAME_                                  \
    {                                                                \
        [mut = (&mutex)]() { mut->unlock_shared(); }                 \
    }

#else

template<class MutexT>
using TracyMutex = MutexT;

#define NOP \
    do      \
    {       \
    } while (false)

#define TRACY_WAIT_CONNECT() NOP
#define TRACY_PUSH_SCOPE() NOP
#define TRACY_POP_SCOPE() NOP
#define TRACY_AUTO_SCOPE() NOP
#define TRACY_SCOPE_NAME(name) NOP
#define TRACY_SCOPE_NAME_FMT(fmt, ...) NOP
#define TRACY_SCOPE_INFO(info) NOP
#define TRACY_SCOPE_INFO_FMT(fmt, ...) NOP
#define TRACY_SCOPE_COLOR(color) NOP

#define TRACY_DECLARE_MUTEX(mutex_type, mutex_name) \
    mutex_type mutex_name                           \
    {                                               \
    }
#define TRACY_SCOPED_LOCK(mutex)        \
    std::lock_guard _TRACY_UNIQUE_NAME_ \
    {                                   \
        mutex                           \
    }
#define TRACY_SCOPED_SHARED_LOCK(mutex)  \
    std::shared_lock _TRACY_UNIQUE_NAME_ \
    {                                    \
        mutex                            \
    }

#endif
