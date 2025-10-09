#pragma once

#include <bit>
#include <compare>
#include <cstdint>
#include <string_view>

std::string_view ProxyPdfVersion();
std::string_view ProxyPdfBuildTime();

struct SemanticVersion
{
    int32_t m_Major{};
    int32_t m_Minor{};
    int32_t m_Bugfix{};

    auto operator<=>(const SemanticVersion&) const = default;
};
SemanticVersion ProxyPdfToSemanticVersion(std::string_view version);

consteval uint64_t ImageCacheFormatVersion()
{
    constexpr char c_Version[8]{ 'P', 'P', 'P', '0', '0', '0', '0', '5' };
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

consteval std::string_view ConfigFormatVersion()
{
    return "PPP00001";
}
