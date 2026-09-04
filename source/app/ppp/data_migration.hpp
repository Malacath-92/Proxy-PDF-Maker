#pragma once

#include <ppp/util.hpp>

class Config;
class PrintProxyPrepApplication;

// Backwards compatiblity of card/page size default saved in config
void MigrateOldConfigDefaults(PrintProxyPrepApplication& app,
                              Config& config);

// Move config file from cwd to standard config folder
void MigrateConfigFromCwd(const fs::path& config_name,
                          const fs::path& config_folder);

// Prompt user to move projects from cwd to standard documents folder
void MigrateProjectsFromCwd(const fs::path& projects_folder);

// Prompt user to move data from cwd to standard data folder
void MigrateResourcesFromCwd(const fs::path& data_folder);
