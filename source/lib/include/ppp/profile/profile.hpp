#pragma once

#include <tracy/Tracy.hpp>

#ifdef PPP_TRACY_PROFILING

#define TRACY_WAIT_CONNECT() \
    do                       \
    {                        \
    } while (!TracyIsConnected)
#define TRACY_NAMED_SCOPE(scope_name) ZoneNamedN(scope_name, #scope_name, true)
#define TRACY_AUTO_SCOPE() ZoneScoped
#else

#define NOP \
    do      \
    {       \
    } while (false)

#define TRACY_WAIT_CONNECT() NOP
#define TRACY_AUTO_SCOPE() NOP
#define TRACY_NAMED_SCOPE(scope_name) NOP

#endif
