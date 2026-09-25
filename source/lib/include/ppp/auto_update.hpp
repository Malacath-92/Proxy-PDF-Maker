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

// Callback on progress, giving "Work Title" and progress
using ProgressFn = std::function<void(std::string_view, float)>;

// Download binaries and extract to a sub-folder
bool AutoUpdateDownloadRelease(std::string_view version,
                               ProgressFn progress_fn = nullptr);

// Try to start the auto-update progress
AutoUpdateConclusion AutoUpdateTryInitialize(std::span<char*> argv);

// Returns true if the program should exit after
// executing the phase, may spawn other processes
using AutoUpdatePhase = bool(std::span<char*> argv);

// Get the current phase in the update process
AutoUpdatePhase* ResolveAutoUpdatePhase(std::span<char*> argv);