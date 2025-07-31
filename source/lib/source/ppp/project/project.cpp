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

        if (json.contains("custom_margins_four"))
        {
            m_Data.m_CustomMarginsFour = FourMargins{
                json["custom_margins_four"]["left"].get<float>() * 1_cm,
                json["custom_margins_four"]["top"].get<float>() * 1_cm,
                json["custom_margins_four"]["right"].get<float>() * 1_cm,
                json["custom_margins_four"]["bottom"].get<float>() * 1_cm,
            };
        }
        else
        {
            m_Data.m_CustomMarginsFour.reset();
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
        if (m_Data.m_CustomMarginsFour.has_value())
        {
            json["custom_margins_four"] = nlohmann::json{
                { "left", m_Data.m_CustomMarginsFour->m_Left / 1_cm },
                { "top", m_Data.m_CustomMarginsFour->m_Top / 1_cm },
                { "right", m_Data.m_CustomMarginsFour->m_Right / 1_cm },
                { "bottom", m_Data.m_CustomMarginsFour->m_Bottom / 1_cm },
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
    LogDebug("Loading preview cache...");
    m_Data.m_Previews = ReadPreviews(m_Data.m_ImageCache);

    InitProperties();
    EnsureOutputFolder();
}

void Project::InitProperties()
{
    LogDebug("Collecting images...");

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
    LogDebug("CacheCardLayout: Starting card layout calculation");

    const bool fit_size{ m_Data.m_PageSize == Config::c_FitSize };
    if (!fit_size)
    {
        LogDebug("CacheCardLayout: Not fit size mode, calculating layout");

        const auto previous_layout{ m_Data.m_CardLayout };
        LogDebug("CacheCardLayout: Previous layout: {}x{}", previous_layout.x, previous_layout.y);

        const Size page_size{ ComputePageSize() };
        LogDebug("CacheCardLayout: Page size calculated");

        const Size card_size_with_bleed{ CardSizeWithBleed() };
        LogDebug("CacheCardLayout: Card size with bleed calculated");

        // Calculate available space after accounting for margins
        Size available_space{ page_size };
        LogDebug("CacheCardLayout: Initial available space set");

        if (m_Data.m_CustomMarginsFour.has_value())
        {
            LogDebug("CacheCardLayout: Using custom margins four");
            const auto margins_four{ ComputeMarginsFour() };
            LogDebug("CacheCardLayout: Margins four calculated");
            available_space.x -= (margins_four.m_Left + margins_four.m_Right);
            available_space.y -= (margins_four.m_Top + margins_four.m_Bottom);
            LogDebug("CacheCardLayout: Available space after margins calculated");
        }
        else if (m_Data.m_CustomMargins.has_value())
        {
            LogDebug("CacheCardLayout: Using custom margins");
            const auto margins{ ComputeMargins() };
            LogDebug("CacheCardLayout: Margins calculated");
            available_space.x -= margins.x * 2;
            available_space.y -= margins.y * 2;
            LogDebug("CacheCardLayout: Available space after margins calculated");
        }

        // Safety check: ensure available space is valid
        if (available_space.x <= 0_mm || available_space.y <= 0_mm)
        {
            LogInfo("CacheCardLayout: Available space invalid, setting layout to 0x0");
            // Margins are too large, no cards can fit
            m_Data.m_CardLayout = dla::uvec2{ 0, 0 };
            return previous_layout != m_Data.m_CardLayout;
        }

        // Safety check: ensure card size is valid
        if (card_size_with_bleed.x <= 0_mm || card_size_with_bleed.y <= 0_mm)
        {
            LogInfo("CacheCardLayout: Card size invalid, setting layout to 0x0");
            // Invalid card size, no cards can fit
            m_Data.m_CardLayout = dla::uvec2{ 0, 0 };
            return previous_layout != m_Data.m_CardLayout;
        }

        // Safety check: ensure we don't divide by zero or very small values
        if (card_size_with_bleed.x < 0.1_mm || card_size_with_bleed.y < 0.1_mm)
        {
            LogInfo("CacheCardLayout: Card size too small, setting layout to 0x0");
            // Card size is too small, no cards can fit
            m_Data.m_CardLayout = dla::uvec2{ 0, 0 };
            return previous_layout != m_Data.m_CardLayout;
        }

        LogDebug("CacheCardLayout: Calculating layout");

        m_Data.m_CardLayout = static_cast<dla::uvec2>(dla::floor(available_space / card_size_with_bleed));
        LogInfo("CacheCardLayout: Calculated layout: {}x{}", m_Data.m_CardLayout.x, m_Data.m_CardLayout.y);

        // Safety check: if no columns, no cards can fit
        if (m_Data.m_CardLayout.x == 0)
        {
            LogInfo("CacheCardLayout: No columns can fit, setting layout to 0x0");
            m_Data.m_CardLayout = dla::uvec2{ 0, 0 };
            return previous_layout != m_Data.m_CardLayout;
        }

        if (m_Data.m_Spacing.x > 0_mm || m_Data.m_Spacing.y > 0_mm)
        {
            LogDebug("CacheCardLayout: Adjusting for spacing");
            const Size cards_size{ ComputeCardsSize() };
            LogDebug("CacheCardLayout: Cards size calculated");
            if (cards_size.x > available_space.x)
            {
                LogDebug("CacheCardLayout: Cards too wide, reducing columns");
                m_Data.m_CardLayout.x--;
            }
            if (cards_size.y > available_space.y)
            {
                LogDebug("CacheCardLayout: Cards too tall, reducing rows");
                m_Data.m_CardLayout.y--;
            }
            LogDebug("CacheCardLayout: Final layout after spacing adjustment: {}x{}", m_Data.m_CardLayout.x, m_Data.m_CardLayout.y);
        }

        // Final safety check: ensure we have at least 1 column
        if (m_Data.m_CardLayout.x == 0)
        {
            LogInfo("CacheCardLayout: Final check - no columns can fit, setting layout to 0x0");
            m_Data.m_CardLayout = dla::uvec2{ 0, 0 };
        }

        LogDebug("CacheCardLayout: Layout calculation complete: {}x{}", m_Data.m_CardLayout.x, m_Data.m_CardLayout.y);
        return previous_layout != m_Data.m_CardLayout;
    }

    LogDebug("CacheCardLayout: Fit size mode, returning false");
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

Size Project::ComputeEffectiveCardsSize() const
{
    LogDebug("ComputeEffectiveCardsSize: Starting calculation");

    const Size page_size{ ComputePageSize() };
    LogDebug("ComputeEffectiveCardsSize: Page size calculated");

    const Size card_size_with_bleed{ CardSizeWithBleed() };
    LogDebug("ComputeEffectiveCardsSize: Card size with bleed calculated");

    const auto [columns, rows]{ m_Data.m_CardLayout.pod() };
    LogDebug("ComputeEffectiveCardsSize: Card layout: {}x{}", columns, rows);

    // Safety check: if no cards can fit, return zero size
    if (columns == 0 || rows == 0)
    {
        LogInfo("ComputeEffectiveCardsSize: No cards can fit, returning zero size");
        return Size{ 0_mm, 0_mm };
    }

    // Calculate the actual size of the cards area (including spacing)
    const Size cards_size{ m_Data.m_CardLayout * card_size_with_bleed + (m_Data.m_CardLayout - 1) * m_Data.m_Spacing };
    LogDebug("ComputeEffectiveCardsSize: Cards size calculated");

    // If custom margins are set, the effective cards size is the cards area plus the margins
    if (m_Data.m_CustomMarginsFour.has_value())
    {
        LogDebug("ComputeEffectiveCardsSize: Using custom margins four");
        const auto margins_four{ ComputeMarginsFour() };
        LogDebug("ComputeEffectiveCardsSize: Margins four calculated");
        const Size effective_size{
            cards_size.x + margins_four.m_Left + margins_four.m_Right,
            cards_size.y + margins_four.m_Top + margins_four.m_Bottom
        };
        LogDebug("ComputeEffectiveCardsSize: Effective size calculated");
        return effective_size;
    }
    else if (m_Data.m_CustomMargins.has_value())
    {
        LogDebug("ComputeEffectiveCardsSize: Using custom margins");
        const auto margins{ ComputeMargins() };
        LogDebug("ComputeEffectiveCardsSize: Margins calculated");
        const Size effective_size{
            cards_size.x + margins.x * 2,
            cards_size.y + margins.y * 2
        };
        LogDebug("ComputeEffectiveCardsSize: Effective size calculated");
        return effective_size;
    }

    // If no custom margins, return the cards size as is
    LogDebug("ComputeEffectiveCardsSize: No custom margins, returning cards size");
    return cards_size;
}

Size Project::ComputeMargins() const
{
    return m_Data.ComputeMargins(g_Cfg);
}

Size Project::ComputeMaxMargins() const
{
    return m_Data.ComputeMaxMargins(g_Cfg);
}

FourMargins Project::ComputeMarginsFour() const
{
    return m_Data.ComputeMarginsFour(g_Cfg);
}

Length Project::ComputeMaxLeftMargin(Length right_margin) const
{
    return m_Data.ComputeMaxLeftMargin(g_Cfg, right_margin);
}

Length Project::ComputeMaxRightMargin(Length left_margin) const
{
    return m_Data.ComputeMaxRightMargin(g_Cfg, left_margin);
}

Length Project::ComputeMaxTopMargin(Length bottom_margin) const
{
    return m_Data.ComputeMaxTopMargin(g_Cfg, bottom_margin);
}

Length Project::ComputeMaxBottomMargin(Length top_margin) const
{
    return m_Data.ComputeMaxBottomMargin(g_Cfg, top_margin);
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
    // Return zero size when no cards can fit (invalid layout)
    if (m_CardLayout.x == 0 || m_CardLayout.y == 0)
    {
        return Size{ 0_mm, 0_mm };
    }

    const Size card_size_with_bleed{ CardSizeWithBleed(config) };
    return m_CardLayout * card_size_with_bleed + (m_CardLayout - 1) * m_Spacing;
}

Size Project::ProjectData::ComputeMargins(const Config& config) const
{
    // Custom margins take precedence over computed defaults to allow user-defined layouts
    // for specific printing requirements or aesthetic preferences
    if (m_CustomMargins.has_value())
    {
        return m_CustomMargins.value();
    }

    // Default to centered margins by dividing available space equally
    // This provides a balanced layout suitable for most printing scenarios
    return ComputeMaxMargins(config) / 2.0f;
}

Size Project::ProjectData::ComputeMaxMargins(const Config& config) const
{
    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    // Maximum margins represent the total available space around the cards
    // This is used to constrain user input and provide reasonable defaults
    // Add 0.01 mm buffer to prevent crashes when values are exactly at the limit
    const Size max_margins{ page_size - cards_size - Size{ 0.01_mm, 0.01_mm } };
    return Size{
        max_margins.x > 0_mm ? max_margins.x : 0_mm,
        max_margins.y > 0_mm ? max_margins.y : 0_mm
    };
}

Length Project::ProjectData::ComputeMaxLeftMargin(const Config& config, Length right_margin) const
{
    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    // Left margin is limited by page width minus cards width minus right margin
    // Add 0.01 mm buffer to prevent crashes when values are exactly at the limit
    const Length max_left{ page_size.x - cards_size.x - right_margin - 0.01_mm };
    // Also ensure we don't return unreasonably large values that could cause issues
    return (max_left > 0_mm && max_left < page_size.x) ? max_left : 0_mm;
}

Length Project::ProjectData::ComputeMaxRightMargin(const Config& config, Length left_margin) const
{
    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    // Right margin is limited by page width minus cards width minus left margin
    // Add 0.01 mm buffer to prevent crashes when values are exactly at the limit
    const Length max_right{ page_size.x - cards_size.x - left_margin - 0.01_mm };
    // Also ensure we don't return unreasonably large values that could cause issues
    return (max_right > 0_mm && max_right < page_size.x) ? max_right : 0_mm;
}

Length Project::ProjectData::ComputeMaxTopMargin(const Config& config, Length bottom_margin) const
{
    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    // Top margin is limited by page height minus cards height minus bottom margin
    // Add 0.01 mm buffer to prevent crashes when values are exactly at the limit
    const Length max_top{ page_size.y - cards_size.y - bottom_margin - 0.01_mm };
    // Also ensure we don't return unreasonably large values that could cause issues
    return (max_top > 0_mm && max_top < page_size.y) ? max_top : 0_mm;
}

Length Project::ProjectData::ComputeMaxBottomMargin(const Config& config, Length top_margin) const
{
    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    // Bottom margin is limited by page height minus cards height minus top margin
    // Add 0.01 mm buffer to prevent crashes when values are exactly at the limit
    const Length max_bottom{ page_size.y - cards_size.y - top_margin - 0.01_mm };
    // Also ensure we don't return unreasonably large values that could cause issues
    return (max_bottom > 0_mm && max_bottom < page_size.y) ? max_bottom : 0_mm;
}

FourMargins Project::ProjectData::ComputeMarginsFour(const Config& config) const
{
    // Custom four-margin settings override computed defaults to support asymmetric layouts
    // needed for professional printing with binding, cutting guides, or aesthetic requirements
    if (m_CustomMarginsFour.has_value())
    {
        return m_CustomMarginsFour.value();
    }

    // Default to centered four-margin layout by dividing available space equally
    // This maintains visual balance while providing individual margin control
    const Size max_margins{ ComputeMaxMargins(config) };
    const Length half_width{ max_margins.x / 2.0f };
    const Length half_height{ max_margins.y / 2.0f };

    return FourMargins{
        .m_Left = half_width,
        .m_Top = half_height,
        .m_Right = half_width,
        .m_Bottom = half_height,
    };
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
