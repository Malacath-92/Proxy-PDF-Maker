#pragma once

#include <bit>
#include <cstdint>
#include <string_view>

std::string_view ProxyPdfVersion();
std::string_view ProxyPdfBuildTime();

consteval uint64_t ImageCacheFormatVersion()
{
    constexpr char c_Version[8]{ 'P', 'P', 'P', '0', '0', '0', '0', '3' };
    return std::bit_cast<uint64_t>(c_Version);
}

consteval std::string_view JsonFormatVersion()
{
    return "PPP00009";
}

consteval std::string_view ImageDbFormatVersion()
{
    return "PPP00002";
}
