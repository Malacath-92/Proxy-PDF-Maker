#pragma once

#include <string>
#include <optional>

struct CommandLineOptions
{
    bool m_HelpDisplayed{ false };

    bool m_NoUI{ false };
    bool m_Render{ false };
    bool m_WaitForCropper{ false };

    std::optional<std::string> m_ProjectFile{ std::nullopt };
    std::optional<std::string> m_ProjectJson{ std::nullopt };
    std::unordered_map<std::string, std::string> m_ProjectOverrides{};
};

CommandLineOptions ParseCommandLine(int argc, char** argv);
