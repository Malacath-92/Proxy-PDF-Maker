#include <ppp/config.hpp>

#include <charconv>
#include <ranges>
#include <string>

#include <QFile>
#include <QSettings>

#include <magic_enum/magic_enum.hpp>

#include <ppp/constants.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

Config CFG{ LoadConfig() };

void Config::SetPdfBackend(PdfBackend backend)
{
    Backend = backend;
    if (Backend != PdfBackend::PoDoFo)
    {
        PageSizes.erase(std::string{ Config::BasePDFSize });
    }
    else
    {
        PageSizes[std::string{ Config::BasePDFSize }] = {};
    }
}

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
        static constexpr auto c_ToStringViews{ std::views::transform(
            [](auto str)
            { return std::string_view(str.data(), str.size()); }) };
        static constexpr auto c_ToFloat{
            [](std::string_view str)
            {
                float val;
#ifdef __clang__
                // Clang and AppleClang do not support std::from_chars overloads with floating points
                val = std::stof(std::string{ str });
#else
                std::from_chars(str.data(), str.data() + str.size(), val);
#endif
                return val;
            },
        };
        static constexpr auto c_GetDecimals{
            [](std::string_view str) -> uint32_t
            {
                auto parts{
                    str |
                    std::views::split('.') |
                    c_ToStringViews |
                    std::ranges::to<std::vector>()
                };
                if (parts.size() != 2)
                {
                    return 0;
                }
                return static_cast<uint32_t>(parts[1].size());
            },
        };

        static constexpr auto c_ParseSize{
            [](std::string str) -> std::optional<Config::SizeInfo>
            {
                std::replace(str.begin(), str.end(), ',', '.');

                const auto parts{
                    str |
                    std::views::split(' ') |
                    c_ToStringViews |
                    std::ranges::to<std::vector>()
                };

                if (parts.size() != 4 || parts[1] != "x")
                {
                    return std::nullopt;
                }

                const auto base_unit_opt{ Config::GetUnitFromName(parts.back()) };
                if (!base_unit_opt.has_value())
                {
                    return std::nullopt;
                }
                const auto base_unit{ base_unit_opt.value().m_Unit };

                const auto width_str{ parts[0] };
                const auto height_str{ parts[2] };

                const auto width{ c_ToFloat(width_str) };
                const auto height{ c_ToFloat(height_str) };

                const auto decimals{ std::max(c_GetDecimals(width_str), c_GetDecimals(height_str)) };

                return Config::SizeInfo{
                    { width * base_unit, height * base_unit },
                    base_unit,
                    decimals,
                };
            },
        };
        static constexpr auto c_ParseLength{
            [](std::string str) -> std::optional<Config::LengthInfo>
            {
                std::replace(str.begin(), str.end(), ',', '.');

                const auto parts{
                    str |
                    std::views::split(' ') |
                    c_ToStringViews |
                    std::ranges::to<std::vector>()
                };

                if (parts.size() != 2)
                {
                    return std::nullopt;
                }

                const auto base_unit_opt{ Config::GetUnitFromName(parts.back()) };
                if (!base_unit_opt.has_value())
                {
                    return std::nullopt;
                }
                const auto base_unit{ base_unit_opt.value().m_Unit };

                const auto length_str{ parts[0] };
                const auto length{ c_ToFloat(length_str) };

                const auto decimals{ c_GetDecimals(length_str) };

                return Config::LengthInfo{
                    length * base_unit,
                    base_unit,
                    decimals,
                };
            },
        };

        {
            settings.beginGroup("DEFAULT");

            config.EnableUncrop = settings.value("Enable.Uncrop", false).toBool();
            config.EnableFancyUncrop = settings.value("Enable.Fancy.Uncrop", true).toBool();
            config.BasePreviewWidth = settings.value("Base.Preview.Width", 248).toInt() * 1_pix;
            config.MaxDPI = settings.value("Max.DPI", 1200).toInt() * 1_dpi;
            config.DisplayColumns = settings.value("Display.Columns", 5).toInt();
            config.DefaultPageSize = settings.value("Page.Size", "Letter").toString().toStdString();
            config.ColorCube = settings.value("Color.Cube", "None").toString().toStdString();

            {
                const auto pdf_backend{ settings.value("PDF.Backend", "LibHaru").toString().toStdString() };
                config.SetPdfBackend(magic_enum::enum_cast<PdfBackend>(pdf_backend)
                                         .value_or(PdfBackend::LibHaru));
            }

            {
                auto pdf_image_format{ settings.value("PDF.Backend.Image.Format", "Png").toString() };
                if (pdf_image_format == "Jpg")
                {
                    config.PdfImageFormat = ImageFormat::Jpg;
                }
                else
                {
                    config.PdfImageFormat = ImageFormat::Png;
                }
            }

            {
                auto png_compression{ settings.value("PDF.Backend.Png.Compression") };
                if (png_compression.isValid())
                {
                    config.PngCompression = std::clamp(png_compression.toInt(), 0, 9);
                }
            }

            {
                auto jpg_quality{ settings.value("PDF.Backend.Jpg.Quality") };
                if (jpg_quality.isValid())
                {
                    config.JpgQuality = std::clamp(jpg_quality.toInt(), 0, 100);
                }
            }

            {
                auto base_unit{ settings.value("Base.Unit") };
                if (base_unit.isValid())
                {
                    config.BaseUnit = Config::GetUnitFromName(base_unit.toString().toStdString())
                                          .value_or(Config::SupportedBaseUnits[0]);
                }
            }

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

                if (auto info{ c_ParseSize(settings.value(key).toString().toStdString()) })
                {
                    config.PageSizes[key.toStdString()] = std::move(info).value();
                }
            }

            settings.endGroup();
        }

        static constexpr auto c_LoadCardSizeInfo{
            [](const QSettings& settings)
            {
                std::optional<Config::CardSizeInfo> full_card_size_info{};
                auto card_size{ settings.value("Card.Size") };
                auto bleed_edge{ settings.value("Input.Bleed") };
                auto corner_radius{ settings.value("Corner.Radius") };
                if (card_size.isValid() && bleed_edge.isValid() && corner_radius.isValid())
                {
                    auto card_size_info{ c_ParseSize(card_size.toString().toStdString()) };
                    auto bleed_edge_info{ c_ParseLength(bleed_edge.toString().toStdString()) };
                    auto corner_radius_info{ c_ParseLength(corner_radius.toString().toStdString()) };
                    if (card_size_info && bleed_edge_info && corner_radius_info)
                    {
                        full_card_size_info.emplace();
                        full_card_size_info->CardSize = std::move(card_size_info).value();
                        full_card_size_info->InputBleed = std::move(bleed_edge_info).value();
                        full_card_size_info->CornerRadius = std::move(corner_radius_info).value();
                        full_card_size_info->CardSizeScale = std::max(settings.value("Card.Scale", 1.0f).toFloat(), 0.0f);
                    };
                }
                return full_card_size_info;
            }
        };

        for (const QString& group : settings.childGroups())
        {
            if (group.startsWith("CARD_SIZE") && group.indexOf("-") != -1)
            {
                settings.beginGroup(group);
                if (auto card_size_info{ c_LoadCardSizeInfo(settings) })
                {
                    const auto group_name_split{ group.split("-") };
                    const QStringList card_size_name_split{ group_name_split.begin() + 1, group_name_split.end() };
                    const auto card_size_name_start{ card_size_name_split.join("-") };
                    auto card_size_name{ card_size_name_start.trimmed().toStdString() };
                    config.CardSizes[std::move(card_size_name)] = std::move(card_size_info).value();
                }
                settings.endGroup();
            }
        }
    }

    return config;
}

void SaveConfig(Config config)
{
    QSettings settings("config.ini", QSettings::IniFormat);
    if (settings.status() == QSettings::Status::NoError)
    {
        static constexpr auto c_SetSize{
            [](QSettings& settings, const std::string& name, const Config::SizeInfo& info, float scale = 1.0f)
            {
                const auto& [size, base, decimals]{ info };
                const auto unit{ Config::GetUnitFromValue(base)
                                     .transform(&UnitInfo::GetName)
                                     .value_or("mm") };
                const auto [width, height]{ (size / base / scale).pod() };
                settings.setValue(name, fmt::format("{0:.{2}f} x {1:.{2}f} {3}", width, height, decimals, unit).c_str());
            }
        };
        static constexpr auto c_SetLength{
            [](QSettings& settings, const std::string& name, const Config::LengthInfo& info, float scale = 1.0f)
            {
                const auto& [length, base, decimals]{ info };
                const auto unit{ Config::GetUnitFromValue(base)
                                     .transform(&UnitInfo::GetName)
                                     .value_or("mm") };
                settings.setValue(name, fmt::format("{0:.{1}f} {2}", length / base / scale, decimals, unit).c_str());
            }
        };

        {
            settings.beginGroup("DEFAULT");

            settings.setValue("Enable.Uncrop", config.EnableUncrop);
            settings.setValue("Base.Preview.Width", config.BasePreviewWidth / 1_pix);
            settings.setValue("Max.DPI", config.MaxDPI / 1_dpi);
            settings.setValue("Display.Columns", config.DisplayColumns);
            settings.setValue("Page.Size", ToQString(config.DefaultPageSize));
            settings.setValue("Color.Cube", ToQString(config.ColorCube));

            const std::string_view pdf_backend{ magic_enum::enum_name(config.Backend) };
            settings.setValue("PDF.Backend", ToQString(pdf_backend));

            const char* pdf_image_format{
                [](ImageFormat image_format)
                {
                    switch (image_format)
                    {
                    case ImageFormat::Jpg:
                        return "Jpg";
                    case ImageFormat::Png:
                    default:
                        return "Png";
                    }
                }(config.PdfImageFormat),
            };
            settings.setValue("PDF.Backend.Image.Format", ToQString(pdf_image_format));

            if (config.PngCompression.has_value())
            {
                settings.setValue("PDF.Backend.Png.Compression", config.PngCompression.value());
            }

            if (config.JpgQuality.has_value())
            {
                settings.setValue("PDF.Backend.Jpg.Quality", config.JpgQuality.value());
            }

            const auto base_unit_name{ config.BaseUnit.m_Name };
            settings.setValue("Base.Unit", ToQString(base_unit_name));

            settings.endGroup();
        }

        {
            settings.beginGroup("PAGE_SIZES");

            for (const auto& [name, info] : config.PageSizes)
            {
                if (name == Config::FitSize || name == Config::BasePDFSize)
                {
                    continue;
                }

                c_SetSize(settings, name, info);
            }

            settings.endGroup();
        }

        static constexpr auto c_WriteCardSizeInfo{
            [](QSettings& settings, const Config::CardSizeInfo& card_size_info)
            {
                c_SetSize(settings, "Card.Size", card_size_info.CardSize);
                c_SetLength(settings, "Input.Bleed", card_size_info.InputBleed);
                c_SetLength(settings, "Corner.Radius", card_size_info.CornerRadius);
                if (static_cast<int32_t>(card_size_info.CardSizeScale * 10000) != 10000)
                {
                    settings.setValue("Card.Scale", card_size_info.CardSizeScale);
                }
            }
        };

        for (const auto& [card_name, card_size_info] : config.CardSizes)
        {
            settings.beginGroup("CARD_SIZE - " + card_name);
            c_WriteCardSizeInfo(settings, card_size_info);
            settings.endGroup();
        }
    }
    settings.sync();
}
