#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader_settings.hpp>

#include <nlohmann/json.hpp>

#include <ppp/app.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/profile/profile.hpp>

MtgDownloaderSettings MtgDownloaderSettings::Read()
{
    TRACY_AUTO_SCOPE();

    const auto& application{ *ppApp };
    const auto settings_file{ application.GetConfigFolder() / "plugins/mtg_downloader.json" };

    if (!fs::exists(settings_file))
    {
        return MtgDownloaderSettings{};
    }

    std::ifstream file_stream{ settings_file };
    std::string json_blob{ std::istreambuf_iterator<char>{ file_stream },
                           std::istreambuf_iterator<char>{} };

    LogInfo("Reading MtG Downloader Plugin settings...");

    try
    {
        const auto json(nlohmann::json::parse(json_blob));

        if (!json.is_object())
        {
            LogError("Settings json type is {}, expected object...\n{}",
                     json.type_name(),
                     json.dump());
            throw std::logic_error{ "Unexpected settings root..." };
        }

        return MtgDownloaderSettings{
            .m_UpscaleModel{ ToQString(json["upscale_model"].get<std::string>()) },
            .m_AdjustSettings = json["adjust_settings"],
            .m_DownloadBacksides = json["download_backsides"],
            .m_ArtCrops = json["art_crops"],
            .m_ClearImages = json["clear_images"],
            .m_FillCorners = json["fill_corners"],
        };
    }
    catch (const std::exception& e)
    {
        LogError("Failed loading MtG Downloader Plugin settings, continuing defaults: {}\n{}", e.what(), json_blob);
    }

    return MtgDownloaderSettings{};
}

void MtgDownloaderSettings::Write()
{
    TRACY_AUTO_SCOPE();

    const auto& application{ *ppApp };
    const auto plugins_folder{ application.GetConfigFolder() / "plugins" };
    if (!fs::exists(plugins_folder))
    {
        fs::create_directories(plugins_folder);
    }

    const auto settings_file{ plugins_folder / "mtg_downloader.json" };

    if (std::ofstream file{ settings_file })
    {
        LogInfo("Generating MtG Downloader Plugin settings json...");

        nlohmann::json json{};
        json["upscale_model"] = m_UpscaleModel.toStdString();
        json["adjust_settings"] = m_AdjustSettings;
        json["download_backsides"] = m_DownloadBacksides;
        json["art_crops"] = m_ArtCrops;
        json["clear_images"] = m_ClearImages;
        json["fill_corners"] = m_FillCorners;

        LogInfo("Writing settings to {}...", settings_file.string());
        file << json.dump();
    }
}
