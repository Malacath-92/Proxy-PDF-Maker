#include <ppp/project/image_database.hpp>

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>

#include <nlohmann/json.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

bool operator!=(const ImageParameters& lhs, const ImageParameters& rhs)
{
    return static_cast<int32_t>(std::floor(lhs.DPI.value)) != static_cast<int32_t>(std::floor(rhs.DPI.value)) ||
           static_cast<int32_t>(std::floor(lhs.Width.value)) != static_cast<int32_t>(std::floor(rhs.Width.value)) ||
           static_cast<int32_t>(std::floor(lhs.CardSize.x / 0.001_mm)) != static_cast<int32_t>(std::floor(rhs.CardSize.x / 0.001_mm)) ||
           static_cast<int32_t>(std::floor(lhs.CardSize.y / 0.001_mm)) != static_cast<int32_t>(std::floor(rhs.CardSize.y / 0.001_mm)) ||
           static_cast<int32_t>(std::floor(lhs.FullBleedEdge / 0.001_mm)) != static_cast<int32_t>(std::floor(rhs.FullBleedEdge / 0.001_mm));
}

void from_json(const nlohmann::json& json, ImageDataBaseEntry& entry)
{
    // Should look something like this, but the lib never sets the object as binary on load
    // const std::vector<uint8_t>& hash{ json["hash"].get_binary() };

    const auto& hash{ json["hash"]["bytes"].get<std::vector<uint8_t>>() };
    entry.SourceHash = QByteArray{
        reinterpret_cast<const char*>(hash.data()),
        static_cast<qsizetype>(hash.size()),
    };
    entry.Params.DPI.value = json["dpi"].get<int32_t>();
    entry.Params.Width = json["width"].get<int32_t>() * 1_pix;
    entry.Params.CardSize.x = json["card_size"]["width"].get<int32_t>() * 0.001_mm;
    entry.Params.CardSize.y = json["card_size"]["height"].get<int32_t>() * 0.001_mm;
    entry.Params.FullBleedEdge = json["card_input_bleed"].get<int32_t>() * 0.001_mm;
}

void to_json(nlohmann::json& json, const ImageDataBaseEntry& entry)
{
    json["hash"] = nlohmann::json::binary_t{
        std::vector<uint8_t>{
            entry.SourceHash.begin(),
            entry.SourceHash.end(),
        },
    };
    json["dpi"] = static_cast<int32_t>(entry.Params.DPI.value),
    json["width"] = static_cast<int32_t>(entry.Params.Width.value),
    json["card_size"] = nlohmann::json{
        {
            "width",
            static_cast<int32_t>(entry.Params.CardSize.x / 0.001_mm),
        },
        {
            "height",
            static_cast<int32_t>(entry.Params.CardSize.y / 0.001_mm),
        },
    };
    json["card_input_bleed"] = static_cast<int32_t>(entry.Params.FullBleedEdge / 0.001_mm);
}

ImageDataBase ImageDataBase::Read(const fs::path& path)
{
    try
    {
        const nlohmann::json json{ nlohmann::json::parse(std::ifstream{ path }) };
        if (!json.contains("version") || !json["version"].is_string() || json["version"].get_ref<const std::string&>() != ImageDbFormatVersion())
        {
            throw std::logic_error{ "Image databse version not compatible with App version..." };
        }

        ImageDataBase image_db{};
        image_db.DataBase = json["db"].get<decltype(image_db.DataBase)>();
        return image_db;
    }
    catch (const std::exception& e)
    {
        fmt::print("{}", e.what());
        // Failed loading image database, continuing with an empty image databse...
        return ImageDataBase{};
    }
}

void ImageDataBase::Write(const fs::path& path)
{
    if (std::ofstream file{ path })
    {
        nlohmann::json json{};
        json["version"] = ImageDbFormatVersion();
        json["db"] = DataBase;

        file << json;
        file.close();
    }
}

QByteArray ImageDataBase::TestEntry(const fs::path& destination, const fs::path& source, ImageParameters params) const
{
    if (!fs::exists(source))
    {
        return {};
    }

    auto get_source_data{
        [&]()
        {
            QFile source_file{ ToQString(source) };
            source_file.open(QFile::ReadOnly);
            return source_file.readAll();
        }
    };
    QByteArray cur_hash{ QCryptographicHash::hash(get_source_data(), QCryptographicHash::Md5) };

    auto it{ DataBase.find(destination) };
    if (it != DataBase.end())
    {
        if (it->second.Params != params)
        {
            return cur_hash;
        }

        QByteArrayView hash{ it->second.SourceHash };
        if (hash != cur_hash)
        {
            return cur_hash;
        }

        return {};
    }
    return cur_hash;
}

void ImageDataBase::PutEntry(const fs::path& destination, QByteArray source_hash, ImageParameters params)
{
    DataBase[destination] = ImageDataBaseEntry{
        .SourceHash{ std::move(source_hash) },
        .Params{ params },
    };
}
