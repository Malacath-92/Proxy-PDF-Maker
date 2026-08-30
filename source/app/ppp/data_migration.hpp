#pragma once

class Config;
class PrintProxyPrepApplication;

// Backwards compatiblity of card/page size default saved in config
void MigrateOldConfigDefaults(PrintProxyPrepApplication& app,
                              Config& config);
