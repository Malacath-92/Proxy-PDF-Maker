#pragma once

#include <ppp/util.hpp>

class Project;
class Config;

struct PdfResults
{
    fs::path m_FrontsidePdf;
    std::optional<fs::path> m_BacksidePdf;
};
PdfResults GeneratePdf(const Project& project, const Config& config, const fs::path& output_folder);

fs::path GenerateTestPdf(const Project& project, const Config& config, const fs::path& output_folder);
