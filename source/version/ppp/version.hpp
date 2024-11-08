#pragma once

#include <bit>
#include <cstdint>
#include <string_view>

std::string_view proxy_pdf_version();
std::string_view proxy_pdf_build_time();

constexpr uint64_t ImageCacheFormatVersion()
{
    inline constexpr char version[8]{ 'P', 'P', 'P', '0', '0', '0', '0', '1' };
    return std::bit_cast<uint64_t>(version);
}
