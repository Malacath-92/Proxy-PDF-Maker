#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader_settings.hpp>

#include <nlohmann/json.hpp>

#include <ppp/app.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/profile/profile.hpp>

YuGiOhDownloaderSettings YuGiOhDownloaderSettings::Read()
{
    TRACY_AUTO_SCOPE();

    const auto& application{ *ppApp };
    const auto settings_file{ application.GetConfigFolder() / "plugins/yugioh_downloader.json" };

    if (!fs::exists(settings_file))
    {
        return YuGiOhDownloaderSettings{};
    }

    std::ifstream file_stream{ settings_file };
    std::string json_blob{ std::istreambuf_iterator<char>{ file_stream },
                           std::istreambuf_iterator<char>{} };

    LogInfo("Reading YuGiOh Downloader Plugin settings...");

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

        return YuGiOhDownloaderSettings{
            .m_UpscaleModel{ ToQString(json["upscale_model"].get<std::string>()) },
            .m_AdjustSettings = json["adjust_settings"],
            .m_ClearImages = json["clear_images"],
        };
    }
    catch (const std::exception& e)
    {
        LogError("Failed loading YuGiOh Downloader Plugin settings, continuing defaults: {}\n{}", e.what(), json_blob);
    }

    return YuGiOhDownloaderSettings{};
}

void YuGiOhDownloaderSettings::Write()
{
    TRACY_AUTO_SCOPE();

    const auto& application{ *ppApp };
    const auto plugins_folder{ application.GetConfigFolder() / "plugins" };
    if (!fs::exists(plugins_folder))
    {
        fs::create_directories(plugins_folder);
    }

    const auto settings_file{ plugins_folder / "yugioh_downloader.json" };

    if (std::ofstream file{ settings_file })
    {
        LogInfo("Generating YuGiOh Downloader Plugin settings json...");

        nlohmann::json json{};
        json["upscale_model"] = m_UpscaleModel.toStdString();
        json["adjust_settings"] = m_AdjustSettings;
        json["clear_images"] = m_ClearImages;

        LogInfo("Writing settings to {}...", settings_file.string());
        file << json.dump();
    }
}
