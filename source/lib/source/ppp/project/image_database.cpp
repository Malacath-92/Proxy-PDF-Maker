#include <ppp/project/image_database.hpp>

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>

#include <nlohmann/json.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

bool operator!=(const ImageParameters& lhs, const ImageParameters& rhs)
{
    return static_cast<int32_t>(std::floor(lhs.m_DPI.value)) != static_cast<int32_t>(std::floor(rhs.m_DPI.value)) ||
           static_cast<int32_t>(std::floor(lhs.m_Width.value)) != static_cast<int32_t>(std::floor(rhs.m_Width.value)) ||
           static_cast<int32_t>(std::floor(lhs.m_CardSize.x / 0.001_mm)) != static_cast<int32_t>(std::floor(rhs.m_CardSize.x / 0.001_mm)) ||
           static_cast<int32_t>(std::floor(lhs.m_CardSize.y / 0.001_mm)) != static_cast<int32_t>(std::floor(rhs.m_CardSize.y / 0.001_mm)) ||
           static_cast<int32_t>(std::floor(lhs.m_FullBleedEdge / 0.001_mm)) != static_cast<int32_t>(std::floor(rhs.m_FullBleedEdge / 0.001_mm)) ||
           lhs.m_Rotation != rhs.m_Rotation;
}

// NOLINTNEXTLINE
void from_json(const nlohmann::json& json, ImageDataBaseEntry& entry)
{
    // Should look something like this, but the lib never sets the object as binary on load
    // const std::vector<uint8_t>& hash{ json["hash"].get_binary() };

    const auto& hash{ json["hash"]["bytes"].get<std::vector<uint8_t>>() };
    entry.m_SourceHash = QByteArray{
        reinterpret_cast<const char*>(hash.data()),
        static_cast<qsizetype>(hash.size()),
    };
    entry.m_Params.m_DPI.value = json["dpi"].get<int32_t>();
    entry.m_Params.m_Width = json["width"].get<int32_t>() * 1_pix;
    entry.m_Params.m_CardSize.x = json["card_size"]["width"].get<int32_t>() * 0.001_mm;
    entry.m_Params.m_CardSize.y = json["card_size"]["height"].get<int32_t>() * 0.001_mm;
    entry.m_Params.m_FullBleedEdge = json["card_input_bleed"].get<int32_t>() * 0.001_mm;
    if (json.contains("rotation"))
    {
        entry.m_Params.m_Rotation = magic_enum::enum_cast<Image::Rotation>(json["rotation"].get_ref<const std::string&>())
                                        .value_or(Image::Rotation::None);
    }
}

// NOLINTNEXTLINE
void to_json(nlohmann::json& json, const ImageDataBaseEntry& entry)
{
    json["hash"] = nlohmann::json::binary_t{
        std::vector<uint8_t>{
            entry.m_SourceHash.begin(),
            entry.m_SourceHash.end(),
        },
    };
    json["dpi"] = static_cast<int32_t>(entry.m_Params.m_DPI.value),
    json["width"] = static_cast<int32_t>(entry.m_Params.m_Width.value),
    json["card_size"] = nlohmann::json{
        {
            "width",
            static_cast<int32_t>(entry.m_Params.m_CardSize.x / 0.001_mm),
        },
        {
            "height",
            static_cast<int32_t>(entry.m_Params.m_CardSize.y / 0.001_mm),
        },
    };
    json["card_input_bleed"] = static_cast<int32_t>(entry.m_Params.m_FullBleedEdge / 0.001_mm);
    json["rotation"] = magic_enum::enum_name(entry.m_Params.m_Rotation);
}

ImageDataBase ImageDataBase::FromFile(const fs::path& path)
{
    try
    {
        const nlohmann::json json{ nlohmann::json::parse(std::ifstream{ path }) };
        if (!json.contains("version") || !json["version"].is_string() || json["version"].get_ref<const std::string&>() != ImageDbFormatVersion())
        {
            throw std::logic_error{ "Image databse version not compatible with App version..." };
        }

        return ImageDataBase{ json["db"].get<DataBaseMap>(), path };
    }
    catch (const std::exception& e)
    {
        fmt::print("{}", e.what());

        // Failed loading image database, continuing with an empty image databse...
        return ImageDataBase{ path };
    }
}

ImageDataBase& ImageDataBase::Read(const fs::path& path)
{
    try
    {
        const nlohmann::json json{ nlohmann::json::parse(std::ifstream{ path }) };
        if (!json.contains("version") || !json["version"].is_string() || json["version"].get_ref<const std::string&>() != ImageDbFormatVersion())
        {
            throw std::logic_error{ "Image databse version not compatible with App version..." };
        }

        std::lock_guard lock{ m_Mutex };
        m_DataBase = json["db"].get<DataBaseMap>();
        m_Path = path;
    }
    catch (const std::exception& e)
    {
        fmt::print("{}", e.what());

        // Failed loading image database, continuing with an empty image databse...
        std::lock_guard lock{ m_Mutex };
        m_DataBase.clear();
        m_Path.clear();
    }

    return *this;
}

void ImageDataBase::Write()
{
    LogInfo("Writing image database to {}", m_Path.string());

    std::lock_guard lock{ m_Mutex };
    if (std::ofstream file{ m_Path })
    {
        nlohmann::json json{};
        json["version"] = ImageDbFormatVersion();
        json["db"] = m_DataBase;

        file << json;
        file.close();
    }
}

bool ImageDataBase::FindEntry(const fs::path& destination) const
{
    std::lock_guard lock{ m_Mutex };
    return m_DataBase.contains(destination);
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

    if (params.m_WillWriteOutput && !fs::exists(destination))
    {
        return cur_hash;
    }

    std::lock_guard lock{ m_Mutex };
    auto it{ m_DataBase.find(destination) };
    if (it != m_DataBase.end())
    {
        if (it->second.m_Params != params)
        {
            return cur_hash;
        }

        QByteArrayView hash{ it->second.m_SourceHash };
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
    std::lock_guard lock{ m_Mutex };
    m_DataBase[destination] = ImageDataBaseEntry{
        .m_SourceHash{ std::move(source_hash) },
        .m_Params{ params },
    };
}

ImageDataBase::ImageDataBase(fs::path path)
    : m_Path{ std::move(path) }
{
}

ImageDataBase::ImageDataBase(DataBaseMap database,
                             fs::path path)
    : m_DataBase{ std::move(database) }
    , m_Path{ std::move(path) }
{
}