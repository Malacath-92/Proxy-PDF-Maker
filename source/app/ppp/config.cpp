#include <ppp/config.hpp>

#include <ppp/constants.hpp>

#include <qsettings.h>

Config CFG{ LoadConfig() };

Config LoadConfig()
{
    Config config{};

    QSettings settings("config.ini", QSettings::IniFormat);
    if (settings.status() == QSettings::Status::NoError)
    {
        settings.beginGroup("DEFAULT");

        config.VibranceBump = settings.value("Vibrance.Bump", false).toBool();
        config.MaxDPI = settings.value("Max.DPI", 1200).toInt();
        config.DefaultPageSize = settings.value("Page.Size", "Letter").toString().toStdString();
        config.EnableUncrop = settings.value("Enable.Uncrop", true).toBool();
        config.DisplayColumns = settings.value("Display.Columns", 5).toInt();

        settings.endGroup();
    }

    return config;
}

void SaveConfig(Config config)
{
    QSettings settings("config.ini", QSettings::IniFormat);
    if (settings.status() == QSettings::Status::NoError)
    {
        settings.beginGroup("DEFAULT");

        settings.setValue("Vibrance.Bump", config.VibranceBump);
        settings.setValue("Max.DPI", config.MaxDPI);
        settings.setValue("Page.Size", QString::fromStdString(config.DefaultPageSize));
        settings.setValue("Enable.Uncrop", config.EnableUncrop);
        settings.setValue("Display.Columns", config.DisplayColumns);

        settings.endGroup();
    }
    settings.sync();
}
