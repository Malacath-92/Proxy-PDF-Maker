#include <ppp/cubes.hpp>

#include <ranges>

#include <ppp/plugins/plugin_interface.hpp>

#include <ppp/plugins/mtg_card_downloader/mtg_card_downloader.hpp>

inline constexpr std::array c_Plugins{
};

std::vector<std::string_view> GetPluginNames(){
    return c_Plugins |
           std::views::transform(&Plugin::m_Name) |
           std::ranges::to<std::vector>();
}

QWidget* InitPlugin(std::string_view plugin_name, Project& project)
{
    for (auto& [name, init, destroy] : c_Plugins)
    {
        if (name == plugin_name)
        {
            return init(project);
        }
    }
    return nullptr;
}

void DestroyPlugin(std::string_view plugin_name, QWidget* plugin_widget)
{
    for (auto& [name, init, destroy] : c_Plugins)
    {
        if (name == plugin_name)
        {
            destroy(plugin_widget);
        }
    }
}
