#pragma once

#include <optional>

#include <ppp/util.hpp>

class Project;

fs::path GeneratePdf(const Project& project, PrintFn print_fn);
