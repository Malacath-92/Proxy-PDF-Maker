#pragma once

#include <ppp/util.hpp>

class Project;

struct PdfResults
{
    fs::path m_FrontsidePdf;
    std::optional<fs::path> m_BacksidePdf;
};
PdfResults GeneratePdf(const Project& project);

fs::path GenerateTestPdf(const Project& project);
