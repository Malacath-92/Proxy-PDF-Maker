#include <ppp/cubes.hpp>

#include <flat_map>
#include <ranges>

#include <ppp/plugins/plugin_interface.hpp>

#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>
#include <ppp/plugins/yugioh_card_downloader/yugioh_card_downloader.hpp>

using namespace std::string_view_literals;
inline constexpr std::array c_PluginNames{
    "MtG Card Downloader"sv,
    "YuGiOh Card Downloader"sv,
};
inline constexpr std::array c_PluginValues{
    Plugin{
        "MtG Card Downloader",
        &InitMtGCardDownloaderPlugin,
        &DestroyMtGCardDownloaderPlugin,
    },
    Plugin{
        "YuGiOh Card Downloader",
        &InitYuGiOhCardDownloaderPlugin,
        &DestroyYuGiOhCardDownloaderPlugin,
    },
};
const std::flat_map<std::string_view,
                    Plugin,
                    std::less<>,
                    decltype(c_PluginNames),
                    decltype(c_PluginValues)>
    g_Plugins{
        std::sorted_unique,
        c_PluginNames,
        c_PluginValues
    };
std::flat_map<std::string_view,
              PluginInterface*>
    g_InstantiatedPlugins{};

std::span<const std::string_view> GetPluginNames()
{
    return c_PluginNames;
}

PluginInterface* InitPlugin(std::string_view plugin_name,
                            Project& project,
                            const Config& config)
{
    if (g_Plugins.contains(plugin_name))
    {
        auto* plugin{ g_Plugins.at(plugin_name).m_Init(project, config) };
        g_InstantiatedPlugins[plugin_name] = plugin;
        return plugin;
    }
    return nullptr;
}

void DestroyPlugin(std::string_view plugin_name, PluginInterface* plugin)
{
    if (g_Plugins.contains(plugin_name))
    {
        g_InstantiatedPlugins.erase(plugin_name);
        return g_Plugins.at(plugin_name).m_Destroy(plugin);
    }
}

PluginInterface* GetPlugin(std::string_view plugin_name)
{
    return g_InstantiatedPlugins.contains(plugin_name)
               ? g_InstantiatedPlugins.at(plugin_name)
               : nullptr;
}
