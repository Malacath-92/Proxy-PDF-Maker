#include <ppp/project/project.hpp>

#include <ranges>

#include <nlohmann/json.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ppp/config.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

#include <ppp/pdf/util.hpp>

#include <ppp/project/image_ops.hpp>

Project::~Project()
{
    // Save preview cache, in case we didn't finish generating previews we want some partial work saved
    WritePreviews(m_Data.m_ImageCache, m_Data.m_Previews);
}

void Project::Load(const fs::path& json_path)
{
    m_Data = ProjectData{};

    LogInfo("Initializing project...");

    try
    {
        const nlohmann::json json{ nlohmann::json::parse(std::ifstream{ json_path }) };
        if (!json.contains("version") || !json["version"].is_string() || json["version"].get_ref<const std::string&>() != JsonFormatVersion())
        {
            if (JsonFormatVersion() == "PPP00007")
            {
                const auto crop_dir{ fs::path{ json["image_dir"].get<std::string>() } / "crop" };
                for (const auto& sub_dir : ListFolders(crop_dir))
                {
                    fs::remove_all(crop_dir / sub_dir);
                }
            }

            throw std::logic_error{ "Project version not compatible with App version..." };
        }

        m_Data.m_ImageDir = json["image_dir"].get<std::string>();
        m_Data.m_CropDir = m_Data.m_ImageDir / "crop";
        m_Data.m_ImageCache = m_Data.m_CropDir / "preview.cache";

        for (const nlohmann::json& card_json : json["cards"])
        {
            CardInfo& card{ m_Data.m_Cards[card_json["name"]] };
            card.m_Num = card_json["num"];
            card.m_Hidden = card_json["hidden"];
            card.m_Backside = card_json["backside"].get<std::string>();
            card.m_BacksideShortEdge = card_json["backside_short_edge"];
        }

        m_Data.m_BleedEdge.value = json["bleed_edge"];
        {
            const auto& spacing{ json["spacing"] };
            if (spacing.is_number())
            {
                m_Data.m_Spacing.x.value = json["spacing"];
                m_Data.m_Spacing.y = m_Data.m_Spacing.x;
            }
            else
            {
                m_Data.m_Spacing = Size{
                    spacing["width"].get<float>() * 1_mm,
                    spacing["height"].get<float>() * 1_mm,
                };
                m_Data.m_SpacingLinked = json["spacing_linked"];
            }
        }
        if (json.contains("corners"))
        {
            m_Data.m_Corners = magic_enum::enum_cast<CardCorners>(json["corners"].get_ref<const std::string&>())
                                   .value_or(CardCorners::Square);
        }

        m_Data.m_BacksideEnabled = json["backside_enabled"];
        m_Data.m_BacksideDefault = json["backside_default"].get<std::string>();
        m_Data.m_BacksideOffset.value = json["backside_offset"];

        m_Data.m_CardSizeChoice = json["card_size"];
        if (!g_Cfg.m_CardSizes.contains(m_Data.m_CardSizeChoice))
        {
            m_Data.m_CardSizeChoice = g_Cfg.m_DefaultCardSize;
        }

        m_Data.m_PageSize = json["page_size"];
        if (!g_Cfg.m_PageSizes.contains(m_Data.m_PageSize))
        {
            m_Data.m_PageSize = g_Cfg.m_DefaultPageSize;
        }

        m_Data.m_BasePdf = json["base_pdf"];
        m_Data.m_Orientation = magic_enum::enum_cast<PageOrientation>(json["orientation"].get_ref<const std::string&>())
                                   .value_or(PageOrientation::Portrait);
        if (json.contains("flip_page_on"))
        {
            m_Data.m_FlipOn = magic_enum::enum_cast<FlipPageOn>(json["flip_page_on"].get_ref<const std::string&>())
                                  .value_or(FlipPageOn::LeftEdge);
        }
        if (m_Data.m_PageSize == Config::c_FitSize)
        {
            m_Data.m_CardLayout.x = json["card_layout"]["width"];
            m_Data.m_CardLayout.y = json["card_layout"]["height"];
        }
        else
        {
            CacheCardLayout();
        }

        if (json.contains("custom_margins"))
        {
            m_Data.m_CustomMargins = Size{
                json["custom_margins"]["width"].get<float>() * 1_cm,
                json["custom_margins"]["height"].get<float>() * 1_cm,
            };
        }
        else
        {
            m_Data.m_CustomMargins.reset();
        }

        m_Data.m_FileName = json["file_name"].get<std::string>();

        m_Data.m_ExportExactGuides = json["export_exact_guides"];
        m_Data.m_EnableGuides = json["enable_guides"];
        m_Data.m_BacksideEnableGuides = json["enable_backside_guides"];
        if (json.contains("corner_guides"))
        {
            m_Data.m_CornerGuides = json["corner_guides"];
        }
        else
        {
            m_Data.m_CornerGuides = m_Data.m_EnableGuides;
        }
        m_Data.m_CrossGuides = json["cross_guides"];
        m_Data.m_ExtendedGuides = json["extended_guides"];
        m_Data.m_GuidesColorA.r = json["guides_color_a"][0];
        m_Data.m_GuidesColorA.g = json["guides_color_a"][1];
        m_Data.m_GuidesColorA.b = json["guides_color_a"][2];
        m_Data.m_GuidesColorB.r = json["guides_color_b"][0];
        m_Data.m_GuidesColorB.g = json["guides_color_b"][1];
        m_Data.m_GuidesColorB.b = json["guides_color_b"][2];
        m_Data.m_GuidesOffset.value = json["guides_offset"];
        m_Data.m_GuidesThickness.value = json["guides_thickness"];
        m_Data.m_GuidesLength.value = json["guides_length"];
    }
    catch (const std::exception& e)
    {
        LogError("Failed loading project from {}, continuing with an empty project: {}", json_path.string(), e.what());
    }

    Init();
}

void Project::Dump(const fs::path& json_path) const
{
    if (std::ofstream file{ json_path })
    {
        LogInfo("Writing project to {}...", json_path.string());

        nlohmann::json json{};
        json["version"] = JsonFormatVersion();

        json["image_dir"] = m_Data.m_ImageDir.string();
        json["img_cache"] = m_Data.m_ImageCache.string();

        std::vector<nlohmann::json> cards;
        for (const auto& [name, card] : m_Data.m_Cards)
        {
            nlohmann::json& card_json{ cards.emplace_back() };
            card_json["name"] = name.string();
            card_json["num"] = card.m_Num;
            card_json["hidden"] = card.m_Hidden;
            card_json["backside"] = card.m_Backside.string();
            card_json["backside_short_edge"] = card.m_BacksideShortEdge;
        }
        json["cards"] = cards;

        json["bleed_edge"] = m_Data.m_BleedEdge.value;
        json["spacing"] = nlohmann::json{
            { "width", m_Data.m_Spacing.x / 1_mm },
            { "height", m_Data.m_Spacing.y / 1_mm },
        };
        json["spacing_linked"] = m_Data.m_SpacingLinked;
        json["corners"] = magic_enum::enum_name(m_Data.m_Corners);

        json["backside_enabled"] = m_Data.m_BacksideEnabled;
        json["backside_default"] = m_Data.m_BacksideDefault.string();
        json["backside_offset"] = m_Data.m_BacksideOffset.value;

        json["card_size"] = m_Data.m_CardSizeChoice;
        json["page_size"] = m_Data.m_PageSize;
        json["base_pdf"] = m_Data.m_BasePdf;
        if (m_Data.m_CustomMargins.has_value())
        {
            json["custom_margins"] = nlohmann::json{
                { "width", m_Data.m_CustomMargins->x / 1_cm },
                { "height", m_Data.m_CustomMargins->y / 1_cm },
            };
        }
        json["card_layout"] = nlohmann::json{
            { "width", m_Data.m_CardLayout.x },
            { "height", m_Data.m_CardLayout.y },
        };
        json["orientation"] = magic_enum::enum_name(m_Data.m_Orientation);
        json["flip_page_on"] = magic_enum::enum_name(m_Data.m_FlipOn);
        json["file_name"] = m_Data.m_FileName.string();

        json["export_exact_guides"] = m_Data.m_ExportExactGuides;
        json["enable_guides"] = m_Data.m_EnableGuides;
        json["enable_backside_guides"] = m_Data.m_BacksideEnableGuides;
        json["corner_guides"] = m_Data.m_CornerGuides;
        json["cross_guides"] = m_Data.m_CrossGuides;
        json["extended_guides"] = m_Data.m_ExtendedGuides;
        json["guides_color_a"] = std::array{ m_Data.m_GuidesColorA.r, m_Data.m_GuidesColorA.g, m_Data.m_GuidesColorA.b };
        json["guides_color_b"] = std::array{ m_Data.m_GuidesColorB.r, m_Data.m_GuidesColorB.g, m_Data.m_GuidesColorB.b };
        json["guides_offset"] = m_Data.m_GuidesOffset.value;
        json["guides_thickness"] = m_Data.m_GuidesThickness.value;
        json["guides_length"] = m_Data.m_GuidesLength.value;

        file << json;
        file.close();
    }
    else
    {
        LogError("Failed opening file {} for write...", json_path.string());
    }
}

void Project::Init()
{
    LogInfo("Loading preview cache...");
    m_Data.m_Previews = ReadPreviews(m_Data.m_ImageCache);

    InitProperties();
    EnsureOutputFolder();
}

void Project::InitProperties()
{
    LogInfo("Collecting images...");

    // Get all image files in the crop directory or the previews
    const std::vector crop_list{
        g_Cfg.m_EnableUncrop ? ListImageFiles(m_Data.m_ImageDir)
                             : ListImageFiles(m_Data.m_ImageDir, m_Data.m_CropDir)
    };

    // Check that we have all our cards accounted for
    for (const auto& img : crop_list)
    {
        if (!m_Data.m_Cards.contains(img) && img != g_Cfg.m_FallbackName)
        {
            m_Data.m_Cards[img] = CardInfo{};
            if (img.string().starts_with("__"))
            {
                m_Data.m_Cards[img].m_Num = 0;
                m_Data.m_Cards[img].m_Hidden = 1;
            }
        }
    }

    // And also check we don't have stale cards in here
    const auto stale_images{
        m_Data.m_Cards |
            std::views::transform([](const auto& item)
                                  { return std::ref(item.first); }) |
            std::views::filter([&](const auto& img)
                               { return !std::ranges::contains(crop_list, img); }) |
            std::ranges::to<std::vector>(),
    };
    for (const fs::path& img : stale_images)
    {
        m_Data.m_Cards.erase(img);
    }
}

void Project::CardAdded(const fs::path& card_name)
{
    if (!m_Data.m_Cards.contains(card_name))
    {
        if (card_name.string().starts_with("__"))
        {
            m_Data.m_Cards[card_name] = CardInfo{
                .m_Num = 0,
                .m_Hidden = 1,
            };
        }
        else
        {
            m_Data.m_Cards[card_name] = CardInfo{
                .m_Num = 1,
                .m_Hidden = 0,
            };
        }
    }
}

void Project::CardRemoved(const fs::path& card_name)
{
    {
        auto it{ m_Data.m_Cards.find(card_name) };
        if (it != m_Data.m_Cards.end() && it->second.m_ForceKeep > 0)
        {
            --it->second.m_ForceKeep;
            return;
        }
    }

    m_Data.m_Cards.erase(card_name);
    m_Data.m_Previews.erase(card_name);
}

void Project::CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name)
{
    if (m_Data.m_Cards.contains(old_card_name))
    {
        m_Data.m_Cards[new_card_name] = std::move(m_Data.m_Cards.at(old_card_name));
        m_Data.m_Cards.erase(old_card_name);
    }

    if (m_Data.m_Previews.contains(old_card_name))
    {
        m_Data.m_Previews[new_card_name] = std::move(m_Data.m_Previews.at(old_card_name));
        m_Data.m_Previews.erase(old_card_name);
    }
}

bool Project::HasPreview(const fs::path& image_name) const
{
    return m_Data.m_Previews.contains(image_name);
}

const Image& Project::GetCroppedPreview(const fs::path& image_name) const
{
    if (m_Data.m_Previews.contains(image_name))
    {
        return m_Data.m_Previews.at(image_name).m_CroppedImage;
    }
    return m_Data.m_FallbackPreview.m_CroppedImage;
}
const Image& Project::GetUncroppedPreview(const fs::path& image_name) const
{
    if (m_Data.m_Previews.contains(image_name))
    {
        return m_Data.m_Previews.at(image_name).m_UncroppedImage;
    }
    return m_Data.m_FallbackPreview.m_UncroppedImage;
}

const Image& Project::GetCroppedBacksidePreview(const fs::path& image_name) const
{
    return GetCroppedPreview(GetBacksideImage(image_name));
}
const Image& Project::GetUncroppedBacksidePreview(const fs::path& image_name) const
{
    return GetUncroppedPreview(GetBacksideImage(image_name));
}

const fs::path& Project::GetBacksideImage(const fs::path& image_name) const
{
    if (m_Data.m_Cards.contains(image_name))
    {
        const CardInfo& card{ m_Data.m_Cards.at(image_name) };
        if (!card.m_Backside.empty())
        {
            return card.m_Backside;
        }
    }
    return m_Data.m_BacksideDefault;
}

bool Project::CacheCardLayout()
{
    const bool fit_size{ m_Data.m_PageSize == Config::c_FitSize };
    if (!fit_size)
    {
        const auto previous_layout{ m_Data.m_CardLayout };

        const Size page_size{ ComputePageSize() };
        const Size card_size_with_bleed{ CardSizeWithBleed() };
        m_Data.m_CardLayout = static_cast<dla::uvec2>(dla::floor(page_size / card_size_with_bleed));

        if (m_Data.m_Spacing.x > 0_mm || m_Data.m_Spacing.y > 0_mm)
        {
            const Size cards_size{ ComputeCardsSize() };
            if (cards_size.x > page_size.x)
            {
                m_Data.m_CardLayout.x--;
            }
            if (cards_size.y > page_size.y)
            {
                m_Data.m_CardLayout.y--;
            }
        }

        return previous_layout != m_Data.m_CardLayout;
    }

    return false;
}

Size Project::ComputePageSize() const
{
    const bool fit_size{ m_Data.m_PageSize == Config::c_FitSize };
    const bool infer_size{ m_Data.m_PageSize == Config::c_BasePDFSize };

    if (fit_size)
    {
        return ComputeCardsSize();
    }
    else if (infer_size)
    {
        return LoadPdfSize(m_Data.m_BasePdf + ".pdf")
            .value_or(g_Cfg.m_PageSizes["A4"].m_Dimensions);
    }
    else
    {
        auto page_size{ g_Cfg.m_PageSizes[m_Data.m_PageSize].m_Dimensions };
        if (m_Data.m_Orientation == PageOrientation::Landscape)
        {
            std::swap(page_size.x, page_size.y);
        }
        return page_size;
    }
}

Size Project::ComputeCardsSize() const
{
    return m_Data.ComputeCardsSize(g_Cfg);
}

Size Project::ComputeMargins() const
{
    return m_Data.ComputeMargins(g_Cfg);
}

Size Project::ComputeMaxMargins() const
{
    return m_Data.ComputeMaxMargins(g_Cfg);
}

float Project::CardRatio() const
{
    return m_Data.CardRatio(g_Cfg);
}

Size Project::CardSize() const
{
    return m_Data.CardSize(g_Cfg);
}

Size Project::CardSizeWithBleed() const
{
    return m_Data.CardSizeWithBleed(g_Cfg);
}

Size Project::CardSizeWithFullBleed() const
{
    return m_Data.CardSizeWithFullBleed(g_Cfg);
}

Length Project::CardFullBleed() const
{
    return m_Data.CardFullBleed(g_Cfg);
}

Length Project::CardCornerRadius() const
{
    return m_Data.CardCornerRadius(g_Cfg);
}

void Project::EnsureOutputFolder() const
{
    const auto output_dir{
        GetOutputDir(m_Data.m_CropDir, m_Data.m_BleedEdge, g_Cfg.m_ColorCube)
    };
    if (!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }
}

Size Project::ProjectData::ComputePageSize(const Config& config) const
{
    const bool fit_size{ m_PageSize == Config::c_FitSize };
    const bool infer_size{ m_PageSize == Config::c_BasePDFSize };

    if (fit_size)
    {
        return ComputeCardsSize(config);
    }
    else if (infer_size)
    {
        return LoadPdfSize(m_BasePdf + ".pdf")
            .value_or(config.m_PageSizes.at("A4").m_Dimensions);
    }
    else
    {
        auto page_size{ config.m_PageSizes.at(m_PageSize).m_Dimensions };
        if (m_Orientation == PageOrientation::Landscape)
        {
            std::swap(page_size.x, page_size.y);
        }
        return page_size;
    }
}

Size Project::ProjectData::ComputeCardsSize(const Config& config) const
{
    const Size card_size_with_bleed{ CardSize(config) + 2 * m_BleedEdge };
    return m_CardLayout * card_size_with_bleed + (m_CardLayout - 1) * m_Spacing;
}

Size Project::ProjectData::ComputeMargins(const Config& config) const
{
    if (m_CustomMargins.has_value())
    {
        return m_CustomMargins.value();
    }

    return ComputeMaxMargins(config) / 2.0f;
}

Size Project::ProjectData::ComputeMaxMargins(const Config& config) const
{
    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    const Size max_margins{ page_size - cards_size };
    return max_margins;
}

float Project::ProjectData::CardRatio(const Config& config) const
{
    const auto& card_size{ CardSize(config) };
    return card_size.x / card_size.y;
}

Size Project::ProjectData::CardSize(const Config& config) const
{
    const auto& card_size_info{
        config.m_CardSizes.contains(m_CardSizeChoice)
            ? config.m_CardSizes.at(m_CardSizeChoice)
            : config.m_CardSizes.at(g_Cfg.m_DefaultCardSize),
    };
    return card_size_info.m_CardSize.m_Dimensions * card_size_info.m_CardSizeScale;
}

Size Project::ProjectData::CardSizeWithBleed(const Config& config) const
{
    const auto& card_size_info{
        config.m_CardSizes.contains(m_CardSizeChoice)
            ? config.m_CardSizes.at(m_CardSizeChoice)
            : config.m_CardSizes.at(config.m_DefaultCardSize),
    };
    return card_size_info.m_CardSize.m_Dimensions * card_size_info.m_CardSizeScale + m_BleedEdge * 2;
}

Size Project::ProjectData::CardSizeWithFullBleed(const Config& config) const
{
    const auto& card_size_info{
        config.m_CardSizes.contains(m_CardSizeChoice)
            ? config.m_CardSizes.at(m_CardSizeChoice)
            : config.m_CardSizes.at(config.m_DefaultCardSize),
    };
    return (card_size_info.m_CardSize.m_Dimensions + card_size_info.m_InputBleed.m_Dimension * 2) * card_size_info.m_CardSizeScale;
}

Length Project::ProjectData::CardFullBleed(const Config& config) const
{
    const auto& card_size_info{
        config.m_CardSizes.contains(m_CardSizeChoice)
            ? config.m_CardSizes.at(m_CardSizeChoice)
            : config.m_CardSizes.at(config.m_DefaultCardSize),
    };
    return card_size_info.m_InputBleed.m_Dimension * card_size_info.m_CardSizeScale;
}

Length Project::ProjectData::CardCornerRadius(const Config& config) const
{
    const auto& card_size_info{
        config.m_CardSizes.contains(m_CardSizeChoice)
            ? config.m_CardSizes.at(m_CardSizeChoice)
            : config.m_CardSizes.at(config.m_DefaultCardSize),
    };
    return card_size_info.m_CornerRadius.m_Dimension * card_size_info.m_CardSizeScale;
}

void Project::SetPreview(const fs::path& image_name, ImagePreview preview)
{
    PreviewUpdated(image_name, preview);
    m_Data.m_Previews[image_name] = std::move(preview);
}

void Project::CropperDone()
{
    WritePreviews(m_Data.m_ImageCache, m_Data.m_Previews);
}
