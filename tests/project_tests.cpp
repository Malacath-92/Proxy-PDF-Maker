#include <catch2/catch_test_macros.hpp>

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
    Project empty_project{};
    empty_project.Data.ImageDir = "no_images";
    empty_project.Data.CropDir = "no_images/crop";
    empty_project.Data.ImageCache = "empty_project.cache";
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
    Project empty_project{};
    empty_project.Data.ImageDir = "no_images";
    empty_project.Data.CropDir = "no_images/crop";
    empty_project.Data.ImageCache = "empty_project.cache";
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
    Project empty_project{};
    REQUIRE_NOTHROW(empty_project.Load("empty_project.json"));
}

TEST_CASE("Non-empty project", "[project_non_empty]")
{
    Project project{};
    project.Data.ImageDir = "some_images";
    project.Data.CropDir = "some_images/crop";
    project.Data.ImageCache = "non_empty_project.cache";
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
    Project project{};
    project.Data.ImageDir = "some_images";
    project.Data.CropDir = "some_images/crop";
    project.Data.ImageCache = "non_empty_project.cache";
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
    Project project{};
    REQUIRE_NOTHROW(project.Load("non_empty_project.json"));
}
