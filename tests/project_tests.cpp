#include <catch2/catch_test_macros.hpp>

#include <fmt/format.h>

#include <ppp/project/project.hpp>

TEST_CASE("Setup folders for tests", "[project_setup_fs]")
{
    fs::create_directories("no_images");
    fs::create_directories("some_images");
    for (size_t i = 0; i < 18; i++)
    {
        fs::copy_file("fallback.png", fmt::format("some_images/image_{}.png", i));
    }

    std::atexit(
        []()
        {
            fs::remove_all("no_images");
            fs::remove_all("some_images");
        });
}

TEST_CASE("Empty project", "[project_empty]")
{
    const Config& config{};
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
    Project empty_project{ config, ".", imaginary_path };
    empty_project.m_Data.m_ImageDir = "no_images";
    empty_project.m_Data.m_CropDir = "no_images/crop";
    empty_project.m_Data.m_UncropDir = "no_images/uncrop";
    empty_project.m_Data.m_ImageCache = "empty_project.cache";
    REQUIRE_NOTHROW(empty_project.Init());
    empty_project.CropperDone();
    REQUIRE(fs::exists("empty_project.cache"));

    std::atexit(
        []()
        {
            fs::remove("empty_project.cache");
        });
}

TEST_CASE("Empty project can be saved", "[project_save_empty]")
{
    const Config& config{};
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
    Project empty_project{ config, ".", imaginary_path };
    empty_project.m_Data.m_ImageDir = "no_images";
    empty_project.m_Data.m_CropDir = "no_images/crop";
    empty_project.m_Data.m_UncropDir = "no_images/uncrop";
    empty_project.m_Data.m_ImageCache = "empty_project.cache";
    REQUIRE_NOTHROW(empty_project.Dump("empty_project.json"));
    REQUIRE(fs::exists("empty_project.json"));
    REQUIRE(fs::exists("empty_project.cache"));

    std::atexit(
        []()
        {
            fs::remove("empty_project.json");
        });
}

TEST_CASE("Empty project can be loaded", "[project_load_empty]")
{
    const Config& config{};
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
    Project empty_project{ config, ".", imaginary_path };
    REQUIRE_NOTHROW(empty_project.Load("empty_project.json"));
}

TEST_CASE("Non-empty project", "[project_non_empty]")
{
    const Config& config{};
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
    Project project{ config, ".", imaginary_path };
    project.m_Data.m_ImageDir = "some_images";
    project.m_Data.m_CropDir = "some_images/crop";
    project.m_Data.m_UncropDir = "no_images/uncrop";
    project.m_Data.m_ImageCache = "non_empty_project.cache";
    REQUIRE_NOTHROW(project.Init());
    project.CropperDone();

    std::atexit(
        []()
        {
            fs::remove("non_empty_project.cache");
        });
}

TEST_CASE("Non-empty project can be saved", "[project_save_non_empty]")
{
    const Config& config{};
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
    Project project{ config, ".", imaginary_path };
    project.m_Data.m_ImageDir = "some_images";
    project.m_Data.m_CropDir = "some_images/crop";
    project.m_Data.m_UncropDir = "no_images/uncrop";
    project.m_Data.m_ImageCache = "non_empty_project.cache";
    project.Init();
    project.CropperDone();
    REQUIRE_NOTHROW(project.Dump("non_empty_project.json"));
    REQUIRE(fs::exists("non_empty_project.json"));
    REQUIRE(fs::exists("non_empty_project.cache"));

    std::atexit(
        []()
        {
            fs::remove("non_empty_project.json");
        });
}

TEST_CASE("Non-empty project can be loaded", "[project_load_non_empty]")
{
    const Config& config{};
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
    Project project{ config, ".", imaginary_path };
    REQUIRE_NOTHROW(project.Load("non_empty_project.json"));
}
