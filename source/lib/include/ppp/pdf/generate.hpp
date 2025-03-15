#pragma once

#include <optional>

#include <ppp/util.hpp>

class Project;

std::optional<fs::path> GeneratePdf(const Project& project, PrintFn print_fn);
