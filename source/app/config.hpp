#pragma once

#include <string>
#include <cstdint>

struct Config
{
    bool VibranceBump{ false };
    bool EnableUncrop{ true };
    uint32_t MaxDPI{ 1200 };
    uint32_t DisplayColumns{ 5 };
    std::string DefaultPageSize{ "Letter" };
};

Config LoadConfig();
void SaveConfig(Config config);

extern Config CFG;
