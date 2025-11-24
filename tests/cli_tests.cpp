#include <catch2/catch_test_macros.hpp>

#include <QCryptographicHash>
#include <QFile>

#include <fmt/format.h>

#include <ppp/util.hpp>
#include <ppp/qt_util.hpp>

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
        " --project"
        " --image_dir"
        " no_images"
    };

    const int ret{ system(command_line) };

    REQUIRE(ret == 0);
    REQUIRE(fs::exists("_printme.pdf"));

    constexpr const char c_ExpectedHash[]{
        "\xa3\x2a\xde\x7a\x23\x5d\xce\xc4\xc2\x72\x8d\x9b\xe1\x1c\xce\x79"
    };
    const auto file_hash{ hash_file("_printme.pdf") };
    REQUIRE(file_hash == c_ExpectedHash);

    AtScopeExit delete_folders{
        []()
        {
            fs::remove_all("no_images");
        }
    };
}
