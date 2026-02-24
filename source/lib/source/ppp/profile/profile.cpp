#include <ppp/profile/profile.hpp>

#ifdef PPP_TRACY_PROFILING

#include <string>
#include <vector>

#include <Tracy/TracyC.h>

using TracySourceLocation = ___tracy_source_location_data;

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
const char* PushString(std::string string)
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

void TracyPushScope(const char* function_name, std::source_location source_info)
{
    const auto src_loc{ ___tracy_alloc_srcloc(source_info.line(),
                                              source_info.file_name(),
                                              strlen(source_info.file_name()),
                                              function_name,
                                              strlen(function_name),
                                              0) };
    GetScopes()
        .emplace_back()
        .m_Zone = ___tracy_emit_zone_begin_alloc(src_loc, true);
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
    TracyScopeName(PushString(std::move(name)));
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
    TracyScopeInfo(PushString(std::move(name)));
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

#endif
