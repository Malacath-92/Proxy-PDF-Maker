#include <ppp/data_migration.hpp>

#include <QMessageBox>
#include <array>

#include <nlohmann/json.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/util.hpp>
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

void MigrateProjectsFromCwd(const fs::path& projects_folder)
{
    const auto project_files{ ListFiles(".", std::array{ ".json"_p }) };
    if (!project_files.empty())
    {
        const auto response{
            QMessageBox::question(
                nullptr,
                "Migrate Projects",
                QString{ "There are %1 projects in your working directory.\n"
                         "Do you want to move them to the documents folder?" }
                    .arg(project_files.size()))
        };
        if (response == QMessageBox::StandardButton::Yes)
        {
            for (const auto& project_file : project_files)
            {
                SafeMove(project_file, projects_folder);
            }
        }
    }
}

void MigrateResourcesFromCwd(const fs::path& data_folder)
{
    const auto res_folder{ "./res" };
    const auto data_files{ CountFiles(res_folder) };
    if (data_files > 0)
    {
        const auto response{
            QMessageBox::question(
                nullptr,
                "Migrate Projects",
                QString{ "There are %1 data files in your working directory.\n"
                         "Do you want to copy them to the data folder?\n"
                         "If they are not copied you have to manually register them again." }
                    .arg(data_files))
        };
        if (response == QMessageBox::StandardButton::Yes)
        {
            ForEachFolder(res_folder,
                          [&](const fs::path& folder)
                          {
                              fs::copy(folder, data_folder / folder.filename(), fs::copy_options::recursive | fs::copy_options::skip_existing);
                          });
        }
    }
}
