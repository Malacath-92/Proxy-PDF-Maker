#pragma once

#include <bit>
#include <cstdint>
#include <string_view>

std::string_view ProxyPdfVersion();
std::string_view ProxyPdfBuildTime();

consteval uint64_t ImageCacheFormatVersion()
{
    constexpr char version[8]{ 'P', 'P', 'P', '0', '0', '0', '0', '2' };
    return std::bit_cast<uint64_t>(version);
}

consteval std::string_view JsonFormatVersion()
{
    return "PPP00002";
}
