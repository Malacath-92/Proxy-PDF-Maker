#pragma once

#ifdef PPP_TRACY_PROFILING

#include <source_location>

#include <fmt/format.h>

#include <ppp/color.hpp>
#include <ppp/util/at_scope_exit.hpp>

void TracyWaitConnect();

void TracyPushScope(const char* function_name, std::source_location = std::source_location::current());
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

#define MERGE_(a, b) a##b
#define LABEL_(a) MERGE_(unique_name_, a)
#define UNIQUE_NAME LABEL_(__LINE__)

#define TRACY_WAIT_CONNECT() TracyWaitConnect()

#define TRACY_PUSH_SCOPE() TracyPushScope(__func__)
#define TRACY_POP_SCOPE() TracyPopScope()
#define TRACY_AUTO_SCOPE()  \
    TRACY_PUSH_SCOPE();     \
    AtScopeExit UNIQUE_NAME \
    {                       \
        TracyPopScope       \
    }

#define TRACY_SCOPE_NAME(name) TracyScopeName(#name)
#define TRACY_SCOPE_NAME_FMT(fmt, ...) TracyScopeNameFmt(fmt, ##__VA_ARGS__)
#define TRACY_SCOPE_INFO(info) TracyScopeInfo(#info)
#define TRACY_SCOPE_INFO_FMT(fmt, ...) TracyScopeInfoFmt(fmt, ##__VA_ARGS__)
#define TRACY_SCOPE_COLOR(color) TracyScopeColor(color)

#else

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

#endif
