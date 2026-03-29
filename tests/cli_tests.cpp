#include <catch2/catch_test_macros.hpp>

#include <QCryptographicHash>
#include <QFile>

#include <fmt/format.h>

#include <ppp/project/project.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

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
    return QCryptographicHash::hash(id_less_data, QCryptographicHash::Md5);
}

template<size_t N>
void TestPdfFile(const fs::path& pdf_path, const char (&expected)[N])
{
    const auto file_hash{ HashPdfFile(pdf_path) };
    REQUIRE(file_hash == QByteArray{ expected, N - 1 });
}

std::ostream& operator<<(std::ostream& os, const QByteArray& value)
{
    for (auto b : value)
    {
        os << fmt::format("\\x{:0>2x}", b);
    }
    return os;
}

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
        "\xae\x1a\x0e\x4d\x90\x1a\x65\xd0\xf2\x5c\xf3\xf4\xa2\xf4\x1b\x5b"
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
        "\xd0\x61\x68\xda\x1c\x0e\xde\xee\x5d\x10\x59\xc5\xb2\xc2\xa2\x20"
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
        "\x59\xa1\xbb\xf0\x3d\x41\xe2\xe0\xc6\xae\xf6\xe8\x3c\xed\x31\xf4"
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
        "\xc1\x23\xc7\x41\x27\x0a\x07\x20\x84\x42\x3f\xb4\x13\x21\x50\x94"
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
        "\xbb\xa2\x96\xa1\x5e\xab\x80\x7e\xca\x77\xd1\x64\x19\x0c\xa4\x11"
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
        "\x78\xb1\x64\x3e\x1a\x9c\x1b\x51\xeb\xf0\x5b\x33\x5d\x39\xae\xe2"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}
