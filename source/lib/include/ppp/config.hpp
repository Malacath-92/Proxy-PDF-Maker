#pragma once

#include <cstdint>
#include <string>

#include <ppp/util.hpp>

struct Config
{
    bool VibranceBump{ false };
    bool EnableUncrop{ true };
    Pixel BasePreviewWidth{ 248_pix };
    PixelDensity MaxDPI{ 1200_dpi };
    uint32_t DisplayColumns{ 5 };
    std::string DefaultPageSize{ "Letter" };
    std::string Theme{ "Default" };
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config CFG;
