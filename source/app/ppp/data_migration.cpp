#include <ppp/data_migration.hpp>

#include <nlohmann/json.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/util/log.hpp>

void MigrateOldConfigDefaults(PrintProxyPrepApplication& app,
                              Config& config)
{
    if (config.m_DefaultCardSize.value_or("Standard") != "Standard")
    {
        app.SetProjectDefault("card_size", config.m_DefaultCardSize.value());
        config.m_DefaultCardSize.reset();
    }

    if (config.m_DefaultPageSize.value_or("Letter") != "Letter")
    {
        app.SetProjectDefault("page_size", config.m_DefaultPageSize.value());
        config.m_DefaultPageSize.reset();
    }
}

void MigrateConfigFromCwd(const fs::path& config_name,
                          const fs::path& config_folder)
{
    if (fs::exists(config_name))
    {
        if (fs::exists(config_folder / config_name))
        {
            LogWarning("{} exists in working directory and config dir {}, won't be migrated...",
                       config_name.string(),
                       config_folder.string());
            return;
        }

        fs::copy(config_name, config_folder);
        fs::remove(config_name);
    }
}
