#include <catch2/catch_test_macros.hpp>

#include <QCryptographicHash>
#include <QFile>

#include <fmt/format.h>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

QByteArray hash_pdf_file(const fs::path& file_path)
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
        "\x30\x62\x6d\xc0\x0c\x7e\xa9\x08\xc3\x01\x7b\x8e\x18\xf0\x9f\xdf"
    };
    const auto file_hash{ hash_pdf_file("_printme.pdf") };
    REQUIRE(file_hash == c_ExpectedHash);

    AtScopeExit delete_folders{
        []()
        {
            fs::remove("_printme.pdf");
            fs::remove_all("no_images");
        }
    };
}
