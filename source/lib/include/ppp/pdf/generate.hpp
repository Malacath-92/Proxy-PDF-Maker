#pragma once

#include <ppp/util.hpp>

class Project;

fs::path GeneratePdf(const Project& project);

fs::path GenerateTestPdf(const Project& project);


