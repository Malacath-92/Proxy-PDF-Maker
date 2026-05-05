#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include <QCryptographicHash>
#include <QFile>

#include <fmt/format.h>

#include <ppp/project/project.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

std::ostream& operator<<(std::ostream& os, const QByteArray& value)
{
    for (auto b : value)
    {
        os << fmt::format("\\x{:0>2x}", b);
    }
    return os;
}

auto SetupImages(
    const fs::path& folder,
    auto image_fill,
    auto project_mod)
{
    fs::create_directories(folder);
    image_fill(folder);

    const fs::path proj_json{ folder.string() + ".json" };
    {
        Project some_images{};
        some_images.m_Data.m_ImageDir = folder;
        some_images.m_Data.m_CropDir = folder / "crop";
        project_mod(some_images);
        some_images.Dump(proj_json);
    }

    return AtScopeExit{
        [=]()
        {
            fs::remove_all(folder);
            fs::remove_all(proj_json);
        }
    };
}

auto SetupNoImages(const fs::path& folder)
{
    return SetupImages(
        folder,
        [](auto&) {},
        [](auto&) {});
}

auto SetupOneImage(const fs::path& folder)
{
    return SetupImages(
        folder,
        [](const fs::path& folder)
        {
            fs::copy_file("fallback.png", folder / "image.png");
        },
        [](auto&) {});
}

auto SetupSomeImages(const fs::path& folder, auto project_mod)
{
    return SetupImages(
        folder,
        [](const fs::path& folder)
        {
            for (size_t i = 0; i < 18; i++)
            {
                fs::copy_file("fallback.png", fmt::format("{}/image_{}.png", folder.string(), i));
            }
        },
        project_mod);
}

auto SetupSomeImages(const fs::path& folder)
{
    return SetupSomeImages(folder, [](auto&) {});
}

auto SetupSomeImagesWithBackside(const fs::path& folder, auto project_mod)
{
    return SetupImages(
        folder,
        [](const fs::path& folder)
        {
            fs::copy_file("fallback.png", fmt::format("{}/__back.png", folder.string()));
            for (size_t i = 0; i < 18; i++)
            {
                fs::copy_file("fallback.png", fmt::format("{}/image_{}.png", folder.string(), i));
            }
        },
        project_mod);
}

auto RunCLI(const char* command_line)
{
    const int ret{ system(command_line) };

    REQUIRE(ret == 0);
    REQUIRE(fs::exists("_printme.pdf"));
    REQUIRE(!fs::exists("proj.json"));
    REQUIRE(!fs::exists("config.ini"));

    return AtScopeExit{
        []()
        {
            fs::remove("_printme.pdf");
        }
    };
}

QByteArray HashPdfFile(const fs::path& file_path)
{
    const auto source_data{
        [&]()
        {
            QFile source_file{ ToQString(file_path) };
            source_file.open(QFile::ReadOnly);
            return source_file.readAll();
        }()
    };
    const auto id_start{ source_data.lastIndexOf("ID[<") };
    const auto id_end{ source_data.indexOf(">]", id_start) };
    const auto id_less_data{ source_data.sliced(0, id_start) +
                             source_data.sliced(id_end + 2) };

    std::stringstream data_str;
    data_str << source_data;
    LogError("{}", data_str.str());

    return QCryptographicHash::hash(id_less_data, QCryptographicHash::Md5);
}

template<size_t N>
void TestPdfFile(const fs::path& pdf_path, const char (&expected)[N])
{
    const auto file_hash{ HashPdfFile(pdf_path) };
    REQUIRE(file_hash == QByteArray{ expected, N - 1 });
}

static constexpr LogFlags log_flags{
    LogFlags::Console
};
static Log main_log{ log_flags, Log::c_MainLogName };

TEST_CASE("Run CLI without any images", "[cli_empty_project]")
{
    const auto no_images{ SetupNoImages("no_images") };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project"
        " --image_dir no_images"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\xb8\x2c\xb6\x2c\xc1\x30\x30\x40\x7e\x24\xbd\xbf\x61\x03\x1b\xea"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}

TEST_CASE("Run CLI with one image", "[cli_one_image]")
{
    const auto one_image{ SetupOneImage("one_image") };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project one_image.json"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\xf4\x44\x11\x3a\x36\x48\x1e\x61\x25\xe5\xc7\xda\xd8\x85\x46\xa0"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}

TEST_CASE("Run CLI with some images", "[cli_some_images]")
{
    const auto some_images{ SetupSomeImages("some_images") };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project some_images.json"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\xfc\xa5\xf3\x8d\x7f\x91\x6b\xc8\x32\x86\x0c\x43\x69\xe7\x86\x50"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}

TEST_CASE("Run CLI with some images with bleed", "[cli_some_images_with_bleed]")
{
    const auto some_images{
        SetupSomeImages("some_images",
                        [](Project& project)
                        {
                            project.m_Data.m_BleedEdge = 1.5_mm;
                        }),
    };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project some_images.json"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\x13\x5d\x71\x10\x92\xf0\x34\x27\x25\x03\x09\x27\xa0\xbe\x65\xb5"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}

TEST_CASE("Run CLI with some images with spacing", "[cli_some_images_with_spacing]")
{
    const auto some_images{
        SetupSomeImages("some_images",
                        [](Project& project)
                        {
                            project.m_Data.m_Spacing.x = 2_mm;
                            project.m_Data.m_Spacing.y = 2_mm;
                        }),
    };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project some_images.json"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\x81\x36\x40\xd1\xa6\x95\xe3\x8d\x0a\x51\xd8\xf6\x27\xb2\xb2\x16"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}

TEST_CASE("Run CLI with some images with spacing and backside",
          "[cli_some_images_with_spacing_backside]")
{
    const auto some_images{
        SetupSomeImagesWithBackside("some_images",
                                    [](Project& project)
                                    {
                                        project.m_Data.m_Spacing.x = 2_mm;
                                        project.m_Data.m_Spacing.y = 2_mm;
                                        project.m_Data.m_BacksideEnabled = true;
                                    }),
    };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project some_images.json"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\x18\xfa\x73\x96\x67\x08\x0f\x56\x1e\x7d\xd8\x9e\xb9\xf3\xfa\x3b"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}

TEST_CASE("Run CLI with some images with spacing and backside bleed",
          "[cli_some_images_with_spacing_backside_bleed]")
{
    const auto some_images{
        SetupSomeImagesWithBackside("some_images",
                                    [](Project& project)
                                    {
                                        project.m_Data.m_Spacing.x = 2_mm;
                                        project.m_Data.m_Spacing.y = 2_mm;
                                        project.m_Data.m_BacksideEnabled = true;
                                        project.m_Data.m_BacksideExtraBleedEdge = 0.5_mm;
                                    }),
    };

    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --ignore-user-defaults"
        " --project some_images.json"
    };

    const auto cli_res{ RunCLI(command_line) };

    constexpr const char c_ExpectedHash[]{
        "\x8e\x72\xc3\xbf\x90\x1c\x82\x07\xd8\xbb\xee\x3e\x71\x4f\xa4\xcb"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}
