#pragma once

#ifdef PPP_TRACY_PROFILING

#include <tracy/Tracy.hpp>

#define TRACY_WAIT_CONNECT() \
    do                       \
    {                        \
    } while (!TracyIsConnected)

#define TRACY_AUTO_SCOPE() ZoneScoped
#define TRACY_AUTO_SCOPE_NAME(name) ZoneName(#name, sizeof(#name))
#define TRACY_AUTO_SCOPE_INFO(fmt, ...) ZoneTextF(fmt, ##__VA_ARGS__)

#else

#define NOP \
    do      \
    {       \
    } while (false)

#define TRACY_WAIT_CONNECT() NOP
#define TRACY_AUTO_SCOPE() NOP
#define TRACY_AUTO_SCOPE_NAME(name) NOP
#define TRACY_AUTO_SCOPE_INFO(fmt, ...) NOP

#endif
