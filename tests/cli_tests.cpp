#include <catch2/catch_test_macros.hpp>

#include <QCryptographicHash>
#include <QFile>

#include <fmt/format.h>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

QByteArray hash_file(const fs::path& file_path)
{
    const auto source_data{
        [&]()
        {
            QFile source_file{ ToQString(file_path) };
            source_file.open(QFile::ReadOnly);
            return source_file.readAll();
        }()
    };
    return QCryptographicHash::hash(source_data, QCryptographicHash::Md5);
}

TEST_CASE("Run CLI without any images", "[cli_empty_project]")
{
    constexpr char command_line[]{
        PROXY_PDF_CLI_EXE
        " --render"
        " --deterministic"
        " --project"
        " --image_dir no_images"
    };

    const int ret{ system(command_line) };

    REQUIRE(ret == 0);
    REQUIRE(fs::exists("_printme.pdf"));
    REQUIRE(!fs::exists("proj.json"));
    REQUIRE(!fs::exists("config.ini"));

    constexpr const char c_ExpectedHash[]{
        "\x76\x40\xb1\x81\xd3\xc8\x7c\xb9\x91\xd9\x6b\x14\xbc\x33\xf6\x72"
    };
    const auto file_hash{ hash_file("_printme.pdf") };
    REQUIRE(file_hash == c_ExpectedHash);

    AtScopeExit delete_folders{
        []()
        {
            fs::remove("_printme.pdf");
            fs::remove_all("no_images");
        }
    };
}
