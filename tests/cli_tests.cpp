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
        "\x7a\xcc\xfb\xca\x9d\xe1\xbf\x65\x53\x4f\x6d\xe7\xc9\x5d\x1e\x7e"
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
        "\x44\x9a\x1d\xb7\x5b\x5a\xe8\x5e\xdf\x66\xa4\xaa\x61\x26\xa7\xbf"
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
        "\xd2\xc2\xe8\xd8\x62\xb1\x13\x2f\xba\x8c\x1d\x44\xdc\x7e\xbf\x66"
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
        "\x6c\x17\xad\x79\x23\x3b\xff\xda\xa6\xcf\x70\xe7\xd0\xb9\x98\x2c"
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
        "\x76\x1d\x10\x05\x0d\xd5\xe0\x11\xeb\xd0\xb1\x34\x90\xce\x31\x3a"
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
        "\xc7\x4d\x85\x9f\xa4\xc2\x91\x3c\xb4\xf8\x6c\xb4\x08\xcc\x6d\xf1"
    };
    TestPdfFile("_printme.pdf", c_ExpectedHash);
}
