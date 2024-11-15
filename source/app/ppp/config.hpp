#pragma once

#include <cstdint>
#include <string>

#include <ppp/util.hpp>

struct Config
{
    bool VibranceBump{ false };
    bool EnableUncrop{ true };
    PixelDensity MaxDPI{ 1200_dpi };
    uint32_t DisplayColumns{ 5 };
    std::string DefaultPageSize{ "Letter" };
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config CFG;
