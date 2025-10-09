#pragma once

#include <optional>
#include <string>

std::optional<std::string> NewAvailableVersion();
std::string ReleaseURL(std::string_view version);
