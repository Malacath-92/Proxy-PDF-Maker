#include <catch2/catch_test_macros.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/project/project.hpp>

TEST_CASE("Generate empty pdf", "[pdf_empty]")
{
    const Config config{};
    Project empty_project{ config };
    empty_project.m_Data.m_FileName = "empty.pdf";
    (void)GeneratePdf(empty_project, config);
    REQUIRE(fs::exists("empty.pdf"));

    std::atexit(
        []()
        {
            fs::remove("empty.pdf");
        });
}
