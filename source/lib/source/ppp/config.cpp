#include <ppp/config.hpp>

#include <ranges>

#include <QFile>
#include <QSettings>

#include <ppp/constants.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

Config CFG{ LoadConfig() };

Config LoadConfig()
{
    Config config{};
    if (!QFile::exists("config.ini"))
    {
        SaveConfig(config);
        return config;
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    if (settings.status() == QSettings::Status::NoError)
    {
        {
            settings.beginGroup("DEFAULT");

            config.EnableUncrop = settings.value("Enable.Uncrop", true).toBool();
            config.BasePreviewWidth = settings.value("Base.Preview.Width", 248).toInt() * 1_pix;
            config.MaxDPI = settings.value("Max.DPI", 1200).toInt() * 1_dpi;
            config.DisplayColumns = settings.value("Display.Columns", 5).toInt();
            config.DefaultPageSize = settings.value("Page.Size", "Letter").toString().toStdString();
            config.ColorCube = settings.value("Color.Cube", "None").toString().toStdString();

            settings.endGroup();
        }

        {
            settings.beginGroup("PAGE_SIZES");

            for (const auto& key : settings.allKeys())
            {
                if (config.PageSizes.contains(key.toStdString()))
                {
                    continue;
                }

                constexpr auto parse_page_size{
                    [](std::string str) -> std::optional<Config::PageSizeInfo>
                    {
                        static constexpr auto to_string_views{ std::views::transform(
                            [](auto str)
                            { return std::string_view(str.data(), str.size()); }) };
                        static constexpr auto to_float{
                            [](std::string_view str)
                            {
                                float val;
                                std::from_chars(str.data(), str.data() + str.size(), val);
                                return val;
                            },
                        };
                        constexpr auto get_decimals{
                            [](std::string_view str) -> uint32_t
                            {
                                auto parts{
                                    str |
                                    std::views::split('.') |
                                    to_string_views |
                                    std::ranges::to<std::vector>()
                                };
                                if (parts.size() != 2)
                                {
                                    return 0;
                                }
                                return static_cast<uint32_t>(parts[1].size());
                            },
                        };

                        std::replace(str.begin(), str.end(), ',', '.');

                        const auto parts{
                            str |
                            std::views::split(' ') |
                            to_string_views |
                            std::ranges::to<std::vector>()
                        };

                        if (parts.size() != 4 || parts[1] != "x")
                        {
                            return std::nullopt;
                        }

                        Length base_unit{};
                        const auto& base_unit_str{ parts.back() };
                        if (base_unit_str == "inches")
                        {
                            base_unit = 1_in;
                        }
                        else if (base_unit_str == "cm")
                        {
                            base_unit = 10_mm;
                        }
                        else if (base_unit_str == "mm")
                        {
                            base_unit = 1_mm;
                        }
                        else
                        {
                            return std::nullopt;
                        }

                        const auto width_str{ parts[0] };
                        const auto height_str{ parts[2] };

                        const auto width{ to_float(width_str) };
                        const auto height{ to_float(height_str) };

                        const auto decimals{ std::max(get_decimals(width_str), get_decimals(height_str)) };

                        return Config::PageSizeInfo{
                            { width * base_unit, height * base_unit },
                            base_unit,
                            decimals,
                        };
                    },
                };

                if (auto info{ parse_page_size(settings.value(key).toString().toStdString()) })
                {
                    config.PageSizes[key.toStdString()] = std::move(info).value();
                }
            }

            settings.endGroup();
        }
    }

    return config;
}

void SaveConfig(Config config)
{
    QSettings settings("config.ini", QSettings::IniFormat);
    if (settings.status() == QSettings::Status::NoError)
    {
        {
            settings.beginGroup("DEFAULT");

            settings.setValue("Enable.Uncrop", config.EnableUncrop);
            settings.setValue("Base.Preview.Width", config.BasePreviewWidth / 1_pix);
            settings.setValue("Max.DPI", config.MaxDPI / 1_dpi);
            settings.setValue("Display.Columns", config.DisplayColumns);
            settings.setValue("Page.Size", ToQString(config.DefaultPageSize));
            settings.setValue("Color.Cube", ToQString(config.ColorCube));

            settings.endGroup();
        }

        {
            settings.beginGroup("PAGE_SIZES");

            for (const auto& [name, info] : config.PageSizes)
            {
                const auto& [size, base, decimals]{ info };
                const bool is_inches{ dla::math::abs(base - 1_in) < 0.0001_in };
                const bool is_cm{ dla::math::abs(base - 10_mm) < 0.0001_mm };
                // clang-format off
                const auto unit{
                    is_inches ? "inches" :
                        is_cm ? "cm"
                              : "mm"
                };
                // clang-format on
                const auto [width, height]{ (size / base).pod() };
                settings.setValue(name, fmt::format("{0:.{2}f} x {1:.{2}f} {3}", width, height, decimals, unit).c_str());
            }

            settings.endGroup();
        }
    }
    settings.sync();
}
