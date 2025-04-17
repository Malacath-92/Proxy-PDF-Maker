#pragma once

#include <ppp/util.hpp>

class Project;

fs::path GeneratePdf(const Project& project, PrintFn print_fn);
