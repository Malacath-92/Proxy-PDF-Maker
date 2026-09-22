#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include <QCryptographicHash>
#include <QFile>

#include <fmt/format.h>

#include <ppp/project/project.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

const Config g_Cfg{};
const fs::path c_ImaginaryPath{ "./the/path/that/dont/exist" };

std::ostream& operator<<(std::ostream& os, const QByteArray& value)
{
    os << "\"";
    for (auto b : value)
    {
        os << fmt::format("\\x{:0>2x}", b);
    }
    os << "\"";
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
        Project some_images{ g_Cfg, ".", c_ImaginaryPath };
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
        [&]() -> QByteArray
        {
            QFile source_file{ ToQString(file_path) };
            if (source_file.open(QFile::ReadOnly))
            {
                return source_file.readAll();
            }

            return QByteArray{};
        }()
    };

    if (source_data.isEmpty())
    {
        return source_data;
    }

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
        "\x1d\xe1\x11\xf8\x24\xde\xfa\x44\x79\x36\x44\xd6\xcd\x99\x5c\x27"
    };
    const fs::path imaginary_path{ "./the/path/that/dont/exist" };
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
        "\xdd\xd4\xac\xbe\xf0\x22\x66\xe5\x09\x60\x81\x7f\xe5\x9b\x6c\x3d"
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
        "\xea\xc9\x1e\xd2\x2f\x6d\x57\xe7\x51\x52\x53\x5a\xba\x01\xa9\x2b"
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
        "\xba\x53\xde\x9b\x04\x64\x7b\x89\x8a\xb5\xf4\x5a\xef\x56\x4c\xde"
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
        "\xbe\x6b\x9d\xb3\xaf\x6b\x77\x10\xe9\xba\x18\x43\x7a\x1a\x58\x1d"
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
        "\xef\xc1\xd0\x67\x10\x39\x40\x3a\x25\x19\x7f\x68\xcd\xd3\x29\xe5"
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
        "\x80\x73\xaf\x7b\x71\xc6\xbc\x1b\x83\xf6\xeb\x70\xd9\x03\xfb\xdd"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}
