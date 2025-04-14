#include <ppp/config.hpp>

#include <charconv>
#include <ranges>

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
        static constexpr auto get_decimals{
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
        static constexpr auto get_base_unit{
            [](std::string_view base_unit_str) -> std::optional<Length>
            {
                if (base_unit_str == "inches")
                {
                    return 1_in;
                }
                else if (base_unit_str == "cm")
                {
                    return 10_mm;
                }
                else if (base_unit_str == "mm")
                {
                    return 1_mm;
                }
                return std::nullopt;
            }
        };

        static constexpr auto parse_size{
            [](std::string str) -> std::optional<Config::SizeInfo>
            {
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

                const auto base_unit_opt{ get_base_unit(parts.back()) };
                if (!base_unit_opt.has_value())
                {
                    return std::nullopt;
                }
                const auto base_unit{ base_unit_opt.value() };

                const auto width_str{ parts[0] };
                const auto height_str{ parts[2] };

                const auto width{ to_float(width_str) };
                const auto height{ to_float(height_str) };

                const auto decimals{ std::max(get_decimals(width_str), get_decimals(height_str)) };

                return Config::SizeInfo{
                    { width * base_unit, height * base_unit },
                    base_unit,
                    decimals,
                };
            },
        };
        static constexpr auto parse_length{
            [](std::string str) -> std::optional<Config::LengthInfo>
            {
                std::replace(str.begin(), str.end(), ',', '.');

                const auto parts{
                    str |
                    std::views::split(' ') |
                    to_string_views |
                    std::ranges::to<std::vector>()
                };

                if (parts.size() != 2)
                {
                    return std::nullopt;
                }

                const auto base_unit_opt{ get_base_unit(parts.back()) };
                if (!base_unit_opt.has_value())
                {
                    return std::nullopt;
                }
                const auto base_unit{ base_unit_opt.value() };

                const auto length_str{ parts[0] };
                const auto length{ to_float(length_str) };

                const auto decimals{ get_decimals(length_str) };

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
            config.EnableStartupCrop = settings.value("Enable.Startup.Crop", true).toBool();
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
                config.CardSizeScale = settings.value("Card.Size.Scale", 1.0f).toFloat();

                auto card_size_without_bleed{ settings.value("Card.Size.Without.Bleed") };
                auto card_size_with_bleed{ settings.value("Card.Size.With.Bleed") };
                if (card_size_without_bleed.isValid() && card_size_with_bleed.isValid())
                {
                    auto without_info{ parse_size(card_size_without_bleed.toString().toStdString()) };
                    auto with_info{ parse_size(card_size_with_bleed.toString().toStdString()) };
                    if (without_info.has_value() && with_info.has_value())
                    {
                        config.CardSizeWithoutBleed = std::move(without_info).value();
                        config.CardSizeWithBleed = std::move(with_info).value();
                    }
                }

                auto card_corner_radius{ settings.value("Card.Corner.Radius") };
                if (card_corner_radius.isValid())
                {
                    if (auto card_corner_radius_info{ parse_length(card_corner_radius.toString().toStdString()) })
                    {
                        config.CardCornerRadius = std::move(card_corner_radius_info).value();
                    }
                }

                config.CardSizeWithoutBleed.Dimensions *= config.CardSizeScale;
                config.CardSizeWithBleed.Dimensions *= config.CardSizeScale;
                config.CardCornerRadius.Dimension *= config.CardSizeScale;
                config.CardRatio = config.CardSizeWithoutBleed.Dimensions.x / config.CardSizeWithoutBleed.Dimensions.y;
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

                if (auto info{ parse_size(settings.value(key).toString().toStdString()) })
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
        static constexpr auto get_unit_name{
            [](const Length& base)
            {
                const bool is_inches{ dla::math::abs(base - 1_in) < 0.0001_in };
                const bool is_cm{ dla::math::abs(base - 10_mm) < 0.0001_mm };
                // clang-format off
                return is_inches ? "inches" :
                           is_cm ? "cm"
                                 : "mm";
                // clang-format on
            }
        };
        static constexpr auto set_size{
            [](QSettings& settings, const std::string& name, const Config::SizeInfo& info, float scale = 1.0f)
            {
                const auto& [size, base, decimals]{ info };
                const auto unit{ get_unit_name(base) };
                const auto [width, height]{ (size / base / scale).pod() };
                settings.setValue(name, fmt::format("{0:.{2}f} x {1:.{2}f} {3}", width, height, decimals, unit).c_str());
            }
        };
        static constexpr auto set_length{
            [](QSettings& settings, const std::string& name, const Config::LengthInfo& info, float scale = 1.0f)
            {
                const auto& [length, base, decimals]{ info };
                const auto unit{ get_unit_name(base) };
                settings.setValue(name, fmt::format("{0:.{1}f} {2}", length / base / scale, decimals, unit).c_str());
            }
        };

        {
            settings.beginGroup("DEFAULT");

            settings.setValue("Enable.Uncrop", config.EnableUncrop);
            settings.setValue("Enable.Startup.Crop", config.EnableStartupCrop);
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
            };

            set_size(settings, "Card.Size.Without.Bleed", config.CardSizeWithoutBleed, config.CardSizeScale);
            set_size(settings, "Card.Size.With.Bleed", config.CardSizeWithBleed, config.CardSizeScale);
            set_length(settings, "Card.Corner.Radius", config.CardCornerRadius, config.CardSizeScale);

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

                set_size(settings, name, info);
            }

            settings.endGroup();
        }
    }
    settings.sync();
}
