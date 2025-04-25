#include <catch2/catch_test_macros.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/project/project.hpp>

TEST_CASE("Generate empty pdf", "[pdf_empty]")
{
    Project empty_project{};
    empty_project.Data.FileName = "empty.pdf";
    (void)GeneratePdf(empty_project);
    REQUIRE(fs::exists("empty.pdf"));

    std::atexit(
        []()
        {
            fs::remove("empty.pdf");
        });
}
