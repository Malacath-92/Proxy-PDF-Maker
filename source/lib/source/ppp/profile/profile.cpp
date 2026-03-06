#include <ppp/profile/profile.hpp>

#ifdef PPP_TRACY_PROFILING

#include <string>
#include <vector>

#include <Tracy/Tracy.hpp>
#include <Tracy/TracyC.h>

using TracySourceLocation = ___tracy_source_location_data;
static_assert(sizeof(TracySourceInfo) == sizeof(TracySourceLocation));

struct TracyScope
{
    TracyCZoneCtx m_Zone;
    std::vector<std::string> m_StringPool;
};
std::vector<TracyScope>& GetScopes()
{
    thread_local auto s_Scopes{
        []()
        {
            std::vector<TracyScope> scopes;
            scopes.reserve(5);
            return scopes;
        }()
    };
    return s_Scopes;
}
TracyCZoneCtx* CurrentZone()
{
    return GetScopes().empty()
               ? nullptr
               : &GetScopes().back().m_Zone;
}
const char* PushScopeString(std::string string)
{
    return GetScopes().empty()
               ? nullptr
               : GetScopes().back().m_StringPool.emplace_back(std::move(string)).c_str();
}

void TracyWaitConnect()
{
    do
    {
    } while (!___tracy_connected());
}

void TracyPushScope(const TracySourceInfo& source_info)
{
    GetScopes()
        .emplace_back()
        .m_Zone = ___tracy_emit_zone_begin((TracySourceLocation*)&source_info, true);
}
void TracyPopScope()
{
    if (auto* scope{ CurrentZone() })
    {
        ___tracy_emit_zone_end(*scope);
        GetScopes().pop_back();
    }
}

void TracyScopeName(const char* name)
{
    if (auto* scope{ CurrentZone() })
    {
        ___tracy_emit_zone_name(*scope, name, strlen(name));
    }
}
void TracyScopeName(std::string name)
{
    TracyScopeName(PushScopeString(std::move(name)));
}

void TracyScopeInfo(const char* info)
{
    if (auto* scope{ CurrentZone() })
    {
        ___tracy_emit_zone_text(*scope, info, strlen(info));
    }
}
void TracyScopeInfo(std::string name)
{
    TracyScopeInfo(PushScopeString(std::move(name)));
}

void TracyScopeColor(uint32_t color)
{
    if (auto* scope{ CurrentZone() })
    {
        ___tracy_emit_zone_color(*scope, color);
    }
}
void TracyScopeColor(ColorRGB8 color)
{
    TracyScopeColor(ColorToInt(color));
}

TracyMutexBase::TracyMutexBase(TracySourceInfo source_info)
    : m_SourceInfo{ source_info }
{
    m_Handle = ___tracy_announce_lockable_ctx((TracySourceLocation*)&m_SourceInfo);
}

TracyMutexBase ::~TracyMutexBase()
{
    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_terminate_lockable_ctx(handle);
}

void TracyMutexBase::pre_lock() const
{
    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_before_lock_lockable_ctx(handle);
}
void TracyMutexBase::post_lock() const
{
    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_after_lock_lockable_ctx(handle);
}
void TracyMutexBase::post_lock(const TracySourceInfo& source_info) const
{
    post_lock();

    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_mark_lockable_ctx(handle, (TracySourceLocation*)&source_info);
}

void TracyMutexBase::post_try_lock(bool acquired) const
{
    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_after_try_lock_lockable_ctx(handle, acquired);
}
void TracyMutexBase::post_try_lock(bool acquired, const TracySourceInfo& source_info) const
{
    post_try_lock(acquired);

    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_mark_lockable_ctx(handle, (TracySourceLocation*)&source_info);
}

void TracyMutexBase::post_unlock() const
{
    auto* handle{ (TracyCLockCtx)m_Handle };
    ___tracy_after_unlock_lockable_ctx(handle);
}

#endif
