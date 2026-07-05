#pragma once

#include <QLibrary>
#include <QProcess>

#include <span>
#include <string_view>

enum class AutoUpdateConclusion
{
    Initiated,
    NoUpdate,
    Error,
};

// Download binaries and extract to a sub-folder
bool AutoUpdateDownloadRelease(std::string_view version);

// Try to start the auto-update progress
AutoUpdateConclusion AutoUpdateTryInitialize(std::span<char*> argv);

// Returns true if the program should exit after
// executing the phase, may spawn other processes
using AutoUpdatePhase = bool(std::span<char*> argv);

// Get the current phase in the update process
AutoUpdatePhase* ResolveAutoUpdatePhase(std::span<char*> argv);