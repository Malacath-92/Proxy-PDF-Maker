#include <ppp/data_migration.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>

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