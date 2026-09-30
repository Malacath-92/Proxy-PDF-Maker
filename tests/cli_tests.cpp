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
        "\xf1\xca\x43\x15\xcb\xcc\x6a\x41\xda\x45\xd3\xd6\xef\x54\x7c\x06"
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
        "\x45\x8c\xe7\xb4\xf0\x54\x10\xa9\x69\x1b\x09\xc6\xd2\x54\x90\x3d"
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
        "\x71\x29\x9d\x07\x03\x59\xf5\xa1\xbf\xff\xb8\x45\x3b\xb0\x5f\x75"
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
        "\x16\x57\xab\x91\xa0\x37\x5c\xa5\xd7\x16\x9b\xf5\x30\x14\x7b\xf3"
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
        "\x6e\x61\x23\xc7\x75\x71\xd7\x8b\x21\x02\x6c\x1d\xe8\x49\x9c\xbb"
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
        "\x0f\xbd\x58\xce\x2a\x4b\x4d\x89\x9e\x80\x5b\x34\x3e\x01\xec\xa6"
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
        "\xf4\x26\xa3\x67\x3a\x28\xf5\x7c\xd9\x62\x80\x16\x26\x52\x0b\xd1"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}
