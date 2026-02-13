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
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

Config g_Cfg{ LoadConfig() };

void Config::SetPdfBackend(PdfBackend backend)
{
    m_Backend = backend;
    if (m_Backend != PdfBackend::PoDoFo)
    {
        m_PageSizes.erase(std::string{ Config::c_BasePDFSize });
    }
    else
    {
        m_PageSizes[std::string{ Config::c_BasePDFSize }] = {};
    }
}

auto GetFirstValidPageSizeIter(const Config& cfg)
{
    if (cfg.m_PageSizes.contains("Letter"))
    {
        return cfg.m_PageSizes.find("Letter");
    }
    if (cfg.m_PageSizes.contains("A4"))
    {
        return cfg.m_PageSizes.find("A4");
    }

    for (auto it{ cfg.m_PageSizes.begin() }; it != cfg.m_PageSizes.end(); ++it)
    {
        const auto& [page_size, info]{ *it };
        if (page_size != Config::c_FitSize && page_size != Config::c_BasePDFSize)
        {
            return it;
        }
    }

    throw std::logic_error{ "No valid page sizes..." };
}
const Config::SizeInfo& Config::GetFirstValidPageSizeInfo() const
{
    return GetFirstValidPageSizeIter(*this)->second;
}
std::string_view Config::GetFirstValidPageSize() const
{
    return GetFirstValidPageSizeIter(*this)->first;
}

auto GetFirstValidCardSizeIter(const Config& cfg)
{
    if (cfg.m_CardSizes.contains("Standard"))
    {
        return cfg.m_CardSizes.find("Standard");
    }

    if (!cfg.m_CardSizes.empty())
    {
        return cfg.m_CardSizes.begin();
    }

    throw std::logic_error{ "No valid card sizes..." };
}
const Config::CardSizeInfo& Config::GetFirstValidCardSizeInfo() const
{
    return GetFirstValidCardSizeIter(*this)->second;
}
std::string_view Config::GetFirstValidCardSize() const
{
    return GetFirstValidCardSizeIter(*this)->first;
}

bool Config::SvgCardSizeAdded(const fs::path& svg_path, Length input_bleed)
{
    if (!fs::exists(svg_path))
    {
        return false;
    }

    m_CardSizes[svg_path.stem().string()] = CardSizeInfo{
        .m_InputBleed{ input_bleed },
        .m_Hint{},
        .m_CardSizeScale = 1.0f,

        .m_SvgInfo{ {
            .m_SvgName{ svg_path.filename().string() },
            .m_Svg{ LoadSvg(svg_path) },
        } }
    };

    return true;
}

Config LoadConfig()
{
    Config config{};
    if (!QFile::exists("config.ini"))
    {
        return config;
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    if (settings.status() == QSettings::Status::NoError)
    {
        {
            settings.beginGroup("DEFAULT");
            if (!settings.value("Config.Version").isValid())
            {
                return config;
            }
            settings.endGroup();
        }

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

                const auto base_unit_opt{ UnitFromName(parts.back()) };
                if (!base_unit_opt.has_value())
                {
                    return std::nullopt;
                }
                const auto base_unit{ base_unit_opt.value() };
                const auto base_unit_value{ UnitValue(base_unit) };

                const auto width_str{ parts[0] };
                const auto height_str{ parts[2] };

                const auto width{ c_ToFloat(width_str) };
                const auto height{ c_ToFloat(height_str) };

                const auto decimals{ std::max(c_GetDecimals(width_str), c_GetDecimals(height_str)) };

                return Config::SizeInfo{
                    { width * base_unit_value, height * base_unit_value },
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

                const auto base_unit_opt{ UnitFromName(parts.back()) };
                if (!base_unit_opt.has_value())
                {
                    return std::nullopt;
                }
                const auto base_unit{ base_unit_opt.value() };
                const auto base_unit_value{ UnitValue(base_unit) };

                const auto length_str{ parts[0] };
                const auto length{ c_ToFloat(length_str) };

                const auto decimals{ c_GetDecimals(length_str) };

                return Config::LengthInfo{
                    length * base_unit_value,
                    base_unit,
                    decimals,
                };
            },
        };

        {
            settings.beginGroup("DEFAULT");

            config.m_AdvancedMode = settings.value("Advanced.Mode", false).toBool();

            config.m_EnableFancyUncrop = settings.value("Enable.Fancy.Uncrop", true).toBool();
            config.m_BasePreviewWidth = settings.value("Base.Preview.Width", 248).toInt() * 1_pix;
            config.m_MaxDPI = settings.value("Max.DPI", 1200).toInt() * 1_dpi;

            {
                const auto card_order{
                    settings.value("Card.Order", "Alphabetical").toString().toStdString()
                };
                const auto card_order_direction{
                    settings.value("Card.Order.Direction", "Ascending").toString().toStdString()
                };
                config.m_CardOrder = magic_enum::enum_cast<CardOrder>(card_order)
                                         .value_or(CardOrder::Alphabetical);
                config.m_CardOrderDirection = magic_enum::enum_cast<CardOrderDirection>(card_order_direction)
                                                  .value_or(CardOrderDirection::Ascending);
            }

            config.m_MaxWorkerThreads = settings.value("Max.Worker.Threads", 6).toUInt();
            config.m_DisplayColumns = settings.value("Display.Columns", 5).toInt();
            if (settings.contains("Page.Size"))
            {
                config.m_DefaultPageSize = settings.value("Page.Size", "Letter").toString().toStdString();
            }
            config.m_ColorCube = settings.value("Color.Cube", "None").toString().toStdString();

            {
                const auto pdf_backend{ settings.value("PDF.Backend", "PoDoFo").toString().toStdString() };
                config.SetPdfBackend(magic_enum::enum_cast<PdfBackend>(pdf_backend)
                                         .value_or(PdfBackend::PoDoFo));
            }

            {
                auto pdf_image_format{ settings.value("PDF.Backend.Image.Format") };
                if (pdf_image_format.isValid())
                {
                    if (pdf_image_format.toString() == "Jpg")
                    {
                        config.m_PdfImageCompression = ImageCompression::Lossy;
                    }
                    else
                    {
                        config.m_PdfImageCompression = ImageCompression::Lossless;
                    }
                }
                else
                {
                    auto pdf_image_compression{ settings.value("PDF.Backend.Image.Compression", "Lossy")
                                                    .toString()
                                                    .toStdString() };
                    config.m_PdfImageCompression = magic_enum::enum_cast<ImageCompression>(pdf_image_compression)
                                                       .value_or(ImageCompression::Lossy);
                }
            }

            {
                auto png_compression{ settings.value("PDF.Backend.Png.Compression") };
                if (png_compression.isValid())
                {
                    config.m_PngCompression = std::clamp(png_compression.toInt(), 0, 9);
                }
            }

            {
                auto jpg_quality{ settings.value("PDF.Backend.Jpg.Quality") };
                if (jpg_quality.isValid())
                {
                    config.m_JpgQuality = std::clamp(jpg_quality.toInt(), 0, 100);
                }
            }

            {
                auto base_unit{ settings.value("Base.Unit") };
                if (base_unit.isValid())
                {
                    config.m_BaseUnit = UnitFromName(base_unit.toString().toStdString())
                                            .value_or(Unit::Millimeter);
                }
            }

            config.m_RenderZeroBleedRoundedEdges = settings.value("Content.Creator.Mode", false).toBool();

            settings.endGroup();
        }

        {
            settings.beginGroup("PLUGINS");

            for (const auto& key : settings.allKeys())
            {
                config.m_PluginsState[key.toStdString()] = true;
            }

            settings.endGroup();
        }

        {
            settings.beginGroup("PAGE_SIZES");

            if (settings.allKeys().size())
            {
                config.m_PageSizes.clear();
                config.m_PageSizes[std::string{ Config::c_FitSize }] =
                    Config::g_DefaultPageSizes.at(std::string{ Config::c_FitSize });
                config.m_PageSizes[std::string{ Config::c_BasePDFSize }] =
                    Config::g_DefaultPageSizes.at(std::string{ Config::c_BasePDFSize });
            }

            for (const auto& key : settings.allKeys())
            {
                if (config.m_PageSizes.contains(key.toStdString()))
                {
                    continue;
                }

                if (auto info{ c_ParseSize(settings.value(key).toString().toStdString()) })
                {
                    config.m_PageSizes[key.toStdString()] = std::move(info).value();
                }
            }

            settings.endGroup();
        }

        static constexpr auto c_LoadCardSizeInfo{
            [](const QSettings& settings)
            {
                std::optional<Config::CardSizeInfo> full_card_size_info{};
                auto bleed_edge{ settings.value("Input.Bleed") };
                auto card_size{ settings.value("Card.Size") };
                auto corner_radius{ settings.value("Corner.Radius") };
                auto svg_name{ settings.value("Svg.Name") };
                if (bleed_edge.isValid())
                {
                    auto bleed_edge_info{ c_ParseLength(bleed_edge.toString().toStdString()) };
                    if (card_size.isValid() && corner_radius.isValid())
                    {
                        auto card_size_info{ c_ParseSize(card_size.toString().toStdString()) };
                        auto corner_radius_info{ c_ParseLength(corner_radius.toString().toStdString()) };
                        if (bleed_edge_info && card_size_info && corner_radius_info)
                        {
                            full_card_size_info.emplace();
                            full_card_size_info->m_InputBleed = std::move(bleed_edge_info).value();

                            full_card_size_info->m_RoundedRect = {
                                .m_CardSize{ std::move(card_size_info).value() },
                                .m_CornerRadius{ std::move(corner_radius_info).value() },
                            };

                            auto hint{ settings.value("Usage.Hint") };
                            if (hint.isValid())
                            {
                                full_card_size_info->m_Hint = hint.toString().toStdString();
                            }
                        }
                    }
                    else if (svg_name.isValid())
                    {
                        auto svg_path{ fs::path{ "res/card_svgs" } / svg_name.toString().toStdString() };
                        if (bleed_edge_info && fs::exists(svg_path))
                        {
                            full_card_size_info.emplace();
                            full_card_size_info->m_InputBleed = std::move(bleed_edge_info).value();

                            full_card_size_info->m_SvgInfo = {
                                .m_SvgName{ svg_name.toString().toStdString() },
                                .m_Svg{ LoadSvg(svg_path) },
                            };
                        }
                    }
                }

                if (full_card_size_info.has_value())
                {
                    full_card_size_info->m_CardSizeScale = std::max(settings.value("Card.Scale", 1.0f).toFloat(), 0.0f);

                    auto hint{ settings.value("Usage.Hint") };
                    if (hint.isValid())
                    {
                        full_card_size_info->m_Hint = hint.toString().toStdString();
                    }
                }

                return full_card_size_info;
            }
        };

        config.m_CardSizes.clear();
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
                    config.m_CardSizes[std::move(card_size_name)] = std::move(card_size_info).value();
                }
                settings.endGroup();
            }
        }

        if (config.m_CardSizes.empty())
        {
            config.m_CardSizes = Config::g_DefaultCardSizes;
        }
    }

    return config;
}

void SaveConfig(Config config)
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.clear();

    if (settings.status() == QSettings::Status::NoError)
    {
        static constexpr auto c_SetSize{
            [](QSettings& settings, const std::string& name, const Config::SizeInfo& info, float scale = 1.0f)
            {
                const auto& [size, base, decimals]{ info };
                const auto unit{ UnitName(base) };
                const auto unit_value{ UnitValue(base) };
                const auto [width, height]{ (size / unit_value / scale).pod() };
                settings.setValue(name, fmt::format("{0:.{2}f} x {1:.{2}f} {3}", width, height, decimals, unit).c_str());
            }
        };
        static constexpr auto c_SetLength{
            [](QSettings& settings, const std::string& name, const Config::LengthInfo& info, float scale = 1.0f)
            {
                const auto& [length, base, decimals]{ info };
                const auto unit{ UnitName(base) };
                const auto unit_value{ UnitValue(base) };
                settings.setValue(name, fmt::format("{0:.{1}f} {2}", length / unit_value / scale, decimals, unit).c_str());
            }
        };

        {
            settings.beginGroup("DEFAULT");

            settings.setValue("Config.Version", ToQString(ConfigFormatVersion()));

            settings.setValue("Advanced.Mode", config.m_AdvancedMode);

            settings.setValue("Base.Preview.Width", config.m_BasePreviewWidth / 1_pix);
            settings.setValue("Max.DPI", config.m_MaxDPI / 1_dpi);

            const std::string_view card_order{ magic_enum::enum_name(config.m_CardOrder) };
            const std::string_view card_order_direction{ magic_enum::enum_name(config.m_CardOrderDirection) };
            settings.setValue("Card.Order", ToQString(card_order));
            settings.setValue("Card.Order.Direction", ToQString(card_order_direction));

            settings.setValue("Max.Worker.Threads", config.m_MaxWorkerThreads);
            settings.setValue("Display.Columns", config.m_DisplayColumns);
            settings.setValue("Color.Cube", ToQString(config.m_ColorCube));

            const std::string_view pdf_backend{ magic_enum::enum_name(config.m_Backend) };
            settings.setValue("PDF.Backend", ToQString(pdf_backend));
            settings.setValue("PDF.Backend.Image.Compression",
                              ToQString(magic_enum::enum_name(config.m_PdfImageCompression)));

            if (config.m_PngCompression.has_value())
            {
                settings.setValue("PDF.Backend.Png.Compression", config.m_PngCompression.value());
            }

            if (config.m_JpgQuality.has_value())
            {
                settings.setValue("PDF.Backend.Jpg.Quality", config.m_JpgQuality.value());
            }

            const auto base_unit_name{ UnitName(config.m_BaseUnit) };
            settings.setValue("Base.Unit", ToQString(base_unit_name));

            settings.setValue("Content.Creator.Mode", config.m_RenderZeroBleedRoundedEdges);

            settings.endGroup();
        }

        if (std::ranges::contains(config.m_PluginsState | std::views::values, true))
        {
            settings.beginGroup("PLUGINS");

            for (const auto& [plugin_name, plugin_state] : config.m_PluginsState)
            {
                if (plugin_state)
                {
                    settings.setValue(ToQString(plugin_name), true);
                }
            }

            settings.endGroup();
        }

        {
            settings.beginGroup("PAGE_SIZES");

            for (const auto& [name, info] : config.m_PageSizes)
            {
                if (name == Config::c_FitSize || name == Config::c_BasePDFSize)
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
                c_SetLength(settings, "Input.Bleed", card_size_info.m_InputBleed);
                if (static_cast<int32_t>(card_size_info.m_CardSizeScale * 10000) != 10000)
                {
                    settings.setValue("Card.Scale", card_size_info.m_CardSizeScale);
                }
                if (!card_size_info.m_Hint.empty())
                {
                    settings.setValue("Usage.Hint", ToQString(card_size_info.m_Hint));
                }

                if (card_size_info.m_RoundedRect.has_value())
                {
                    c_SetSize(settings, "Card.Size", card_size_info.m_RoundedRect->m_CardSize);
                    c_SetLength(settings, "Corner.Radius", card_size_info.m_RoundedRect->m_CornerRadius);
                }
                else if (card_size_info.m_SvgInfo.has_value())
                {
                    settings.setValue("Svg.Name", ToQString(card_size_info.m_SvgInfo->m_SvgName));
                }
            }
        };

        for (const auto& [card_name, card_size_info] : config.m_CardSizes)
        {
            settings.beginGroup("CARD_SIZE - " + card_name);
            c_WriteCardSizeInfo(settings, card_size_info);
            settings.endGroup();
        }
    }
    settings.sync();
}
