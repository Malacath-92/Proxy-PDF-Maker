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
