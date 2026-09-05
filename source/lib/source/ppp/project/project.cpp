#include <ppp/project/project.hpp>

#include <QTimer>

#include <cmath>
#include <ranges>
#include <utility>

#include <nlohmann/json.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ppp/config.hpp>
#include <ppp/json_util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

#include <ppp/pdf/util.hpp>

#include <ppp/project/image_ops.hpp>

#include <ppp/profile/profile.hpp>

static std::function<bool(const CardInfo&, const CardInfo&)> GetSortFunction(const ConfigData& config)
{
    switch (config.m_CardOrder)
    {
    case CardOrder::Alphabetical:
        switch (config.m_CardOrderDirection)
        {
        case CardOrderDirection::Ascending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_Name < rhs.m_Name;
            };
        case CardOrderDirection::Descending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_Name > rhs.m_Name;
            };
        }
        std::unreachable();
    case CardOrder::Backside:
        switch (config.m_CardOrderDirection)
        {
        case CardOrderDirection::Ascending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_Backside < rhs.m_Backside;
            };
        case CardOrderDirection::Descending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_Backside > rhs.m_Backside;
            };
        }
        std::unreachable();
    case CardOrder::LastModified:
        switch (config.m_CardOrderDirection)
        {
        case CardOrderDirection::Ascending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_LastWriteTime > rhs.m_LastWriteTime;
            };
        case CardOrderDirection::Descending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_LastWriteTime < rhs.m_LastWriteTime;
            };
        }
        std::unreachable();
    case CardOrder::LastAdded:
        switch (config.m_CardOrderDirection)
        {
        case CardOrderDirection::Ascending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_TimeAdded > rhs.m_TimeAdded;
            };
        case CardOrderDirection::Descending:
            return [](const CardInfo& lhs, const CardInfo& rhs)
            {
                return lhs.m_TimeAdded < rhs.m_TimeAdded;
            };
        }
    }
    std::unreachable();
}

static fs::file_time_type TryGetLastWriteTime(const fs::path& file_path)
{
    try
    {
        return fs::last_write_time(file_path);
    }
    catch (const std::exception& e)
    {
        LogError("Failed getting last write time: {}", e.what());
        return {};
    }
}

static bool RoughlyEqual(Angle lhs, Angle rhs)
{
    return dla::math::abs(rhs - lhs) < 0.001_deg;
}

static bool RoughlyEqual(Length lhs, Length rhs)
{
    return dla::math::abs(rhs - lhs) < 0.001_mm;
}

static bool RoughlyEqual(Size lhs, Size rhs)
{
    return RoughlyEqual(lhs.x, rhs.x) &&
           RoughlyEqual(lhs.y, rhs.y);
}

Project::Project(const Config& config,
                 const fs::path& default_projects_folder,
                 const fs::path& base_pdfs_folder)
    : m_Data{ config }
    , m_Cfg{ config }
{
    m_Data.m_ImageDir = default_projects_folder / m_Data.m_ImageDir;
    m_Data.m_CropDir = default_projects_folder / m_Data.m_CropDir;
    m_Data.m_UncropDir = default_projects_folder / m_Data.m_UncropDir;
    m_Data.m_ImageCache = default_projects_folder / m_Data.m_ImageCache;

    m_Data.m_BasePdfsFolder = base_pdfs_folder;
}

Project::~Project()
{
    TRACY_AUTO_SCOPE();

    // Save preview cache, in case we didn't finish generating previews we want some partial work saved
    WritePreviews(m_Data.m_ImageCache, m_Data.m_Previews);
}

bool Project::Load(const fs::path& json_path)
{
    return Load(json_path, {});
}
bool Project::Load(const fs::path& json_path,
                   const JsonProvider* overrides)
{
    TRACY_AUTO_SCOPE();

    std::ifstream file_stream{ json_path };
    std::string json{ std::istreambuf_iterator<char>{ file_stream },
                      std::istreambuf_iterator<char>{} };
    if (!LoadFromJson(json, overrides))
    {
        LogError("Failed loading project from {}...", json_path.string());
        return false;
    }
    return true;
}

bool Project::LoadFromJson(const std::string& json_blob,
                           const JsonProvider* overrides)
{
    const auto old_data{ std::move(m_Data) };
    m_Data = ProjectData{ m_Cfg };
    m_Data.m_ImageDir = old_data.m_ImageDir;
    m_Data.m_CropDir = old_data.m_CropDir;
    m_Data.m_UncropDir = old_data.m_UncropDir;
    m_Data.m_ImageCache = old_data.m_ImageCache;
    m_Data.m_BasePdfsFolder = old_data.m_BasePdfsFolder;

    LogInfo("Initializing project...");

    bool error{ false };
    try
    {
        // Using curly-braces for initializers here makes gcc and clang
        // create an array-of-array, hence we are forced to use parens
        // This is true in multiple places in this function, marked with no-{}
        const auto json(nlohmann::json::parse(json_blob));

        if (!json.is_object())
        {
            LogError("Project json type is {}, expected object...\n{}",
                     json.type_name(),
                     json.dump());
            throw std::logic_error{ "Unexpected project root..." };
        }

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

            if (!json.contains("version"))
            {
                LogError("Project version missing, not compatible with App version {}...",
                         JsonFormatVersion());
            }
            else if (!json["version"].is_string())
            {
                LogError("Project version of type {} not compatible with App version {}...",
                         json["version"].type_name(),
                         JsonFormatVersion());
            }
            else
            {
                LogError("Project version {} not compatible with App version {}...",
                         json["version"].get_ref<const std::string&>(),
                         JsonFormatVersion());
            }
            throw std::logic_error{ "Project version mismatch..." };
        }

        auto get_value{
            [&json, &overrides](std::string_view path,
                                nlohmann::json default_value = {})
                -> nlohmann::json
            {
                if (overrides != nullptr)
                {
                    // no-{}
                    auto value(overrides->GetJsonValue(path));
                    if (!value.is_null())
                    {
                        return value;
                    }
                }

                try
                {
                    const auto& value{ GetJsonValue(json, path) };
                    if (!value.is_null())
                    {
                        return value;
                    }

                    return default_value;
                }
                catch (...)
                {
                    return default_value;
                }
            }
        };

        m_Data.m_ImageDir = get_value("image_dir").get<std::string>();
        m_Data.m_CropDir = m_Data.m_ImageDir / "crop";
        m_Data.m_UncropDir = m_Data.m_ImageDir / "uncrop";
        m_Data.m_ImageCache = m_Data.m_CropDir / "preview.cache";

        // Note: Not using get_value as we don't support overriding card values right now
        for (const nlohmann::json& card_json : json["cards"])
        {
            CardInfo& card{ PutCard(card_json["name"]) };
            card.m_Num = card_json["num"];
            card.m_Hidden = card_json["hidden"];
            if (card_json.contains("backside"))
            {
                card.m_Backside = card_json["backside"].get<std::string>();
            }
            else
            {
                card.m_Backside.reset();
            }
            card.m_BacksideShortEdge = card_json["backside_short_edge"];
            if (card_json.contains("backside_auto_assigned"))
            {
                card.m_BacksideAutoAssigned = card_json["backside_auto_assigned"];
            }
            if (card_json.contains("rotation"))
            {
                card.m_Rotation = magic_enum::enum_cast<Image::Rotation>(card_json["rotation"].get_ref<const std::string&>())
                                      .value_or(Image::Rotation::None);
            }
            if (card_json.contains("bleed_type"))
            {
                card.m_BleedType = magic_enum::enum_cast<BleedType>(card_json["bleed_type"].get_ref<const std::string&>())
                                       .value_or(BleedType::Default);
            }
            if (card_json.contains("ratio_handling"))
            {
                card.m_BadAspectRatioHandling = magic_enum::enum_cast<BadAspectRatioHandling>(card_json["ratio_handling"].get_ref<const std::string&>())
                                                    .value_or(BadAspectRatioHandling::Default);
            }
            if (card_json.contains("external_path"))
            {
                card.m_ExternalPath = card_json["external_path"].get_ref<const std::string&>();
            }
            if (card_json.contains("time_added"))
            {
                card.m_TimeAdded = CardInfoTimePoint{
                    std::chrono::seconds{
                        card_json["time_added"].get<uint64_t>(),
                    }
                };
            }
        }

        {
            // no-{}
            const auto cards_order(get_value("cards_order"));
            if (!cards_order.is_null())
            {
                for (const size_t idx : cards_order)
                {
                    if (idx >= m_Data.m_Cards.size())
                    {
                        m_Data.m_CardsList.clear();
                        LogError("Cards sorting could not be read correctly, invalid index {} contained.",
                                 idx);
                        break;
                    }
                    m_Data.m_CardsList.push_back(m_Data.m_Cards[idx].m_Name);
                }
            }
        }

        {
            // no-{}
            const auto bleed_edge_length(get_value("bleed_edge_cm"));
            if (!bleed_edge_length.is_null())
            {
                m_Data.m_BleedEdge = bleed_edge_length.get<float>() * 1_cm;
            }
            else
            {
                m_Data.m_BleedEdge.value = get_value("bleed_edge");
            }
        }
        {
            // no-{}
            const auto envelope_bleed_edge(get_value("envelope_bleed_edge_cm"));
            if (!envelope_bleed_edge.is_null())
            {
                m_Data.m_EnvelopeBleedEdge = envelope_bleed_edge.get<float>() * 1_cm;
            }
        }
        {
            // no-{}
            const auto& spacing(get_value("spacing"));
            if (spacing.is_number())
            {
                m_Data.m_Spacing.x.value = spacing;
                m_Data.m_Spacing.y = m_Data.m_Spacing.x;
            }
            else
            {
                // no-{}
                auto spacing_width(get_value("spacing.width"));
                auto spacing_height(get_value("spacing.height"));
                if (!spacing_width.is_null() && !spacing_height.is_null())
                {
                    m_Data.m_Spacing = Size{
                        spacing_width.get<float>() * 1_mm,
                        spacing_height.get<float>() * 1_mm,
                    };
                }
                else
                {
                    m_Data.m_Spacing = Size{
                        get_value("spacing.horizontal").get<float>() * 1_cm,
                        get_value("spacing.vertical").get<float>() * 1_cm,
                    };
                }
                m_Data.m_SpacingLinked = get_value("spacing_linked");
            }
        }
        {
            // no-{}
            const auto corners(get_value("corners"));
            if (!corners.is_null())
            {
                m_Data.m_Corners = magic_enum::enum_cast<CardCorners>(corners.get_ref<const std::string&>())
                                       .value_or(CardCorners::Square);
            }
        }

        m_Data.m_BacksideEnabled = get_value("backside_enabled");
        {
            // no-{}
            const auto separate_backsides(get_value("separate_backsides"));
            if (!separate_backsides.is_null())
            {
                m_Data.m_SeparateBacksides = separate_backsides;
            }
        }
        {
            // no-{}
            auto backside_default(get_value("backside_default"));
            if (!backside_default.is_null())
            {
                m_Data.m_BacksideDefault = backside_default.get<std::string>();
            }
            else
            {
                m_Data.m_BacksideDefault.reset();
            }
        }
        {
            // no-{}
            auto backside_offset(get_value("backside_offset"));
            if (backside_offset.is_number())
            {
                m_Data.m_BacksideOffset.x.value = backside_offset;
                m_Data.m_BacksideOffset.y = 0_mm;
            }
            else
            {
                // no-{}
                auto backside_offset_width(get_value("backside_offset.width"));
                auto backside_offset_height(get_value("backside_offset.height"));
                if (!backside_offset_width.is_null() && !backside_offset_height.is_null())
                {
                    m_Data.m_BacksideOffset.x = backside_offset_width.get<float>() * 1_mm;
                    m_Data.m_BacksideOffset.y = backside_offset_height.get<float>() * 1_mm;
                }
                else
                {
                    m_Data.m_BacksideOffset.x = get_value("backside_offset.horizontal").get<float>() * 1_cm;
                    m_Data.m_BacksideOffset.y = get_value("backside_offset.vertical").get<float>() * 1_cm;
                }
            }
        }
        {
            // no-{}
            auto backside_rotation(get_value("backside_rotation"));
            if (backside_rotation.is_number())
            {
                m_Data.m_BacksideRotation = backside_rotation.get<float>() * 1_deg;
            }
        }
        {
            // no-{}
            auto backside_bleed(get_value("backside_bleed"));
            if (backside_bleed.is_number())
            {
                m_Data.m_BacksideExtraBleedEdge = backside_bleed.get<float>() * 1_cm;
            }
        }
        {
            // no-{}
            const auto backside_auto_pattern(get_value("backside_auto_pattern"));
            if (!backside_auto_pattern.is_null())
            {
                m_Data.m_BacksideAutoPattern = backside_auto_pattern;
            }
        }

        m_Data.m_CardSizeChoice = get_value("card_size");
        if (!m_Cfg.m_CardSizes.contains(m_Data.m_CardSizeChoice))
        {
            m_Data.m_CardSizeChoice = m_Cfg.GetFirstValidCardSize();
        }

        m_Data.m_PageSize = get_value("page_size");
        if (!m_Cfg.m_PageSizes.contains(m_Data.m_PageSize))
        {
            m_Data.m_PageSize = m_Cfg.GetFirstValidPageSize();
        }

        m_Data.m_BasePdf = get_value("base_pdf");
        m_Data.m_Orientation = magic_enum::enum_cast<PageOrientation>(get_value("orientation").get_ref<const std::string&>())
                                   .value_or(PageOrientation::Portrait);
        {
            // no-{}
            const auto flip_page_on(get_value("flip_page_on"));
            if (!flip_page_on.is_null())
            {
                m_Data.m_FlipOn = magic_enum::enum_cast<FlipPageOn>(flip_page_on.get_ref<const std::string&>())
                                      .value_or(FlipPageOn::LeftEdge);
            }
        }

        {
            // no-{}
            const auto card_orientation(get_value("card_orientation"));
            if (!card_orientation.is_null())
            {
                m_Data.m_CardOrientation = magic_enum::enum_cast<CardOrientation>(card_orientation.get_ref<const std::string&>())
                                               .value_or(CardOrientation::Vertical);
                m_Data.m_CardLayoutVertical.x = get_value("card_layout_vertical.width");
                m_Data.m_CardLayoutVertical.y = get_value("card_layout_vertical.height");
                m_Data.m_CardLayoutHorizontal.x = get_value("card_layout_horizontal.width");
                m_Data.m_CardLayoutHorizontal.y = get_value("card_layout_horizontal.height");
            }
            else if (m_Data.m_PageSize == Config::c_FitSize)
            {
                m_Data.m_CardLayoutVertical.x = get_value("card_layout.width");
                m_Data.m_CardLayoutVertical.y = get_value("card_layout.height");
            }
        }

        {
            // no-{}
            const auto skipped_layout_slots(get_value("skipped_layout_slots"));
            if (!skipped_layout_slots.is_null())
            {
                m_Data.m_SkippedLayoutSlots = skipped_layout_slots.get<std::vector<size_t>>();
            }
            else
            {
                m_Data.m_SkippedLayoutSlots.clear();
            }
        }

        {
            // no-{}
            const auto custom_margins_width(get_value("custom_margins.width"));
            const auto custom_margins_height(get_value("custom_margins.height"));
            if (!custom_margins_width.is_null() && !custom_margins_height.is_null())
            {
                m_Data.m_CustomMargins.emplace();

                // Legacy two-value margins
                m_Data.m_CustomMargins.value().m_TopLeft = Size{
                    custom_margins_width.get<float>() * 1_cm,
                    custom_margins_height.get<float>() * 1_cm,
                };
            }
            else
            {
                // no-{}
                const auto custom_margins_left(get_value("custom_margins.left"));
                const auto custom_margins_top(get_value("custom_margins.top"));
                if (!custom_margins_left.is_null() && !custom_margins_top.is_null())
                {
                    m_Data.m_CustomMargins.emplace();
                    m_Data.m_MarginsMode = magic_enum::enum_cast<MarginsMode>(json["margins_mode"].get_ref<const std::string&>())
                                               .value_or(MarginsMode::Simple);

                    // Full four-value margins ...
                    m_Data.m_CustomMargins.value().m_TopLeft = Size{
                        custom_margins_left.get<float>() * 1_cm,
                        custom_margins_top.get<float>() * 1_cm,
                    };

                    // ... last two being optional
                    // no-{}
                    const auto custom_margins_right(get_value("custom_margins.right"));
                    const auto custom_margins_bottom(get_value("custom_margins.bottom"));
                    if (!custom_margins_right.is_null() && !custom_margins_bottom.is_null())
                    {
                        m_Data.m_CustomMargins.value().m_BottomRight = Size{
                            custom_margins_right.get<float>() * 1_cm,
                            custom_margins_bottom.get<float>() * 1_cm,
                        };
                    }
                }
                else
                {
                    // No custom margins
                    m_Data.m_CustomMargins.reset();
                }
            }
        }

        CacheCardLayout();

        m_Data.m_FileName = get_value("file_name").get<std::string>();

        {
            // no-{}
            const auto render_header(get_value("render_header"));
            if (!render_header.is_null())
            {
                m_Data.m_RenderPageHeader = render_header;
            }
        }

        m_Data.m_ExportExactGuides = get_value("export_exact_guides");
        m_Data.m_EnableGuides = get_value("enable_guides");
        m_Data.m_BacksideEnableGuides = get_value("enable_backside_guides");
        {
            // no-{}
            const auto corner_guides(get_value("corner_guides"));
            if (!corner_guides.is_null())
            {
                m_Data.m_CornerGuides = corner_guides;
            }
            else
            {
                m_Data.m_CornerGuides = m_Data.m_EnableGuides;
            }
        }
        m_Data.m_CrossGuides = get_value("cross_guides");
        m_Data.m_ExtendedGuides = get_value("extended_guides");
        {
            // no-{}
            const auto& guides_color_a(get_value("guides_color_a"));
            m_Data.m_GuidesColorA.r = guides_color_a[0];
            m_Data.m_GuidesColorA.g = guides_color_a[1];
            m_Data.m_GuidesColorA.b = guides_color_a[2];
        }
        {
            // no-{}
            const auto& guides_color_b(get_value("guides_color_b"));
            m_Data.m_GuidesColorB.r = guides_color_b[0];
            m_Data.m_GuidesColorB.g = guides_color_b[1];
            m_Data.m_GuidesColorB.b = guides_color_b[2];
        }
        {
            // no-{}
            const auto guides_offset_length(get_value("guides_offset_cm"));
            if (!guides_offset_length.is_null())
            {
                m_Data.m_GuidesOffset = guides_offset_length.get<float>() * 1_cm;
            }
            else
            {
                m_Data.m_GuidesOffset.value = get_value("guides_offset");
            }
            // no-{}
            const auto guides_tickness_length(get_value("guides_thickness_cm"));
            if (!guides_tickness_length.is_null())
            {
                m_Data.m_GuidesThickness = guides_tickness_length.get<float>() * 1_cm;
            }
            else
            {
                m_Data.m_GuidesThickness.value = get_value("guides_thickness");
            }
            // no-{}
            const auto guides_length_length(get_value("guides_length_cm"));
            if (!guides_length_length.is_null())
            {
                m_Data.m_GuidesLength = guides_length_length.get<float>() * 1_cm;
            }
            else
            {
                m_Data.m_GuidesLength.value = get_value("guides_length");
            }
        }
    }
    catch (const std::exception& e)
    {
        error = true;
        LogError("Failed loading project, continuing with an empty project: {}\n{}", e.what(), json_blob);
    }

    Init();

    NewProjectOpened(old_data, m_Data);
    return !error;
}

void Project::Dump(const fs::path& json_path) const
{
    TRACY_AUTO_SCOPE();

    if (std::ofstream file{ json_path })
    {
        LogInfo("Generating project json...");
        const auto json_blob{ DumpToJson() };

        LogInfo("Writing project to {}...", json_path.string());
        file << json_blob;
    }
    else
    {
        LogError("Failed opening file {} for write...", json_path.string());
    }
}

std::string Project::DumpToJson() const
{
    return Project::DumpToJson(m_Data);
}
std::string Project::DumpToJson(const ProjectData& data)
{
    TRACY_AUTO_SCOPE();

    nlohmann::json json{};
    json["version"] = JsonFormatVersion();

    json["image_dir"] = data.m_ImageDir.string();
    json["img_cache"] = data.m_ImageCache.string();

    std::vector<nlohmann::json> cards;
    for (const auto& card : data.m_Cards)
    {
        if (!card.m_Transient)
        {
            nlohmann::json& card_json{ cards.emplace_back() };
            card_json["name"] = card.m_Name.string();
            card_json["num"] = card.m_Num;
            card_json["hidden"] = card.m_Hidden;
            if (card.m_Backside.has_value())
            {
                card_json["backside"] = card.m_Backside.value().string();
            }
            card_json["backside_short_edge"] = card.m_BacksideShortEdge;
            card_json["backside_auto_assigned"] = card.m_BacksideAutoAssigned;
            card_json["rotation"] = magic_enum::enum_name(card.m_Rotation);
            card_json["bleed_type"] = magic_enum::enum_name(card.m_BleedType);
            card_json["ratio_handling"] = magic_enum::enum_name(card.m_BadAspectRatioHandling);
            if (card.m_ExternalPath.has_value())
            {
                card_json["external_path"] = card.m_ExternalPath.value().string();
            }
            card_json["time_added"] = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    card.m_TimeAdded.time_since_epoch())
                    .count());
        }
    }
    json["cards"] = cards;

    if (!data.m_CardsList.empty() && data.m_CardsList != data.GenerateDefaultCardsSorting())
    {
        std::vector<size_t> cards_list;
        cards_list.reserve(data.m_CardsList.size());
        for (const auto& name : data.m_CardsList)
        {
            const auto idx{
                data.FindCard(name) - &data.m_Cards.front()
            };
            cards_list.push_back(static_cast<size_t>(idx));
        }

        json["cards_order"] = cards_list;
    }
    else
    {
        json["cards_order"] = std::array<int, 0>{};
    }

    json["bleed_edge_cm"] = data.m_BleedEdge / 1_cm;
    json["envelope_bleed_edge_cm"] = data.m_EnvelopeBleedEdge / 1_cm;
    json["spacing"] = nlohmann::json{
        { "horizontal", data.m_Spacing.x / 1_cm },
        { "vertical", data.m_Spacing.y / 1_cm },
    };
    json["spacing_linked"] = data.m_SpacingLinked;
    json["corners"] = magic_enum::enum_name(data.m_Corners);

    json["backside_enabled"] = data.m_BacksideEnabled;
    json["separate_backsides"] = data.m_SeparateBacksides;

    if (data.m_BacksideDefault.has_value())
    {
        json["backside_default"] = data.m_BacksideDefault.value().string();
    }
    json["backside_offset"] = nlohmann::json{
        { "horizontal", data.m_BacksideOffset.x / 1_cm },
        { "vertical", data.m_BacksideOffset.y / 1_cm },
    };
    json["backside_rotation"] = data.m_BacksideRotation / 1_deg;
    json["backside_bleed"] = data.m_BacksideExtraBleedEdge / 1_cm;
    json["backside_auto_pattern"] = data.m_BacksideAutoPattern;

    json["card_size"] = data.m_CardSizeChoice;
    json["page_size"] = data.m_PageSize;
    json["base_pdf"] = data.m_BasePdf;
    json["margins_mode"] = magic_enum::enum_name(data.m_MarginsMode);
    if (data.m_CustomMargins.has_value())
    {
        if (!data.m_CustomMargins->m_BottomRight.has_value())
        {
            json["custom_margins"] = nlohmann::json{
                { "left", data.m_CustomMargins->m_TopLeft.x / 1_cm },
                { "top", data.m_CustomMargins->m_TopLeft.y / 1_cm },
            };
        }
        else
        {
            json["custom_margins"] = nlohmann::json{
                { "left", data.m_CustomMargins->m_TopLeft.x / 1_cm },
                { "top", data.m_CustomMargins->m_TopLeft.y / 1_cm },
                { "right", data.m_CustomMargins->m_BottomRight->x / 1_cm },
                { "bottom", data.m_CustomMargins->m_BottomRight->y / 1_cm },
            };
        }
    }
    else
    {
        json["custom_margins"] = nlohmann::json{ nlohmann::json::value_t::object };
    }
    json["card_orientation"] = magic_enum::enum_name(data.m_CardOrientation);
    json["card_layout_vertical"] = nlohmann::json{
        { "width", data.m_CardLayoutVertical.x },
        { "height", data.m_CardLayoutVertical.y },
    };
    json["card_layout_horizontal"] = nlohmann::json{
        { "width", data.m_CardLayoutHorizontal.x },
        { "height", data.m_CardLayoutHorizontal.y },
    };
    json["skipped_layout_slots"] = data.m_SkippedLayoutSlots;
    json["orientation"] = magic_enum::enum_name(data.m_Orientation);
    json["flip_page_on"] = magic_enum::enum_name(data.m_FlipOn);
    json["file_name"] = data.m_FileName.string();
    json["render_header"] = data.m_RenderPageHeader;

    json["export_exact_guides"] = data.m_ExportExactGuides;
    json["enable_guides"] = data.m_EnableGuides;
    json["enable_backside_guides"] = data.m_BacksideEnableGuides;
    json["corner_guides"] = data.m_CornerGuides;
    json["cross_guides"] = data.m_CrossGuides;
    json["extended_guides"] = data.m_ExtendedGuides;
    json["guides_color_a"] = std::array{ data.m_GuidesColorA.r, data.m_GuidesColorA.g, data.m_GuidesColorA.b };
    json["guides_color_b"] = std::array{ data.m_GuidesColorB.r, data.m_GuidesColorB.g, data.m_GuidesColorB.b };
    json["guides_offset_cm"] = data.m_GuidesOffset / 1_cm;
    json["guides_thickness_cm"] = data.m_GuidesThickness / 1_cm;
    json["guides_length_cm"] = data.m_GuidesLength / 1_cm;

    return json.dump();
}

bool Project::DiffersWithFile(const fs::path& json_path) const
{
    if (!fs::exists(json_path))
    {
        return true;
    }

    std::ifstream file_stream{ json_path };
    const std::string file_json{ std::istreambuf_iterator<char>{ file_stream },
                                 std::istreambuf_iterator<char>{} };
    const auto project_json{ DumpToJson() };
    return file_json != project_json;
}

void Project::Init()
{
    TRACY_AUTO_SCOPE();

    LogInfo("Loading preview cache...");
    m_Data.m_Previews = ReadPreviews(m_Data.m_ImageCache, m_Cfg.m_FallbackName);
    m_Data.m_FallbackPreview = m_Data.m_Previews.at(m_Cfg.m_FallbackName);

    InitProperties();
    EnsureOutputFolder();
}

void Project::InitProperties()
{
    TRACY_AUTO_SCOPE();

    LogInfo("Collecting images...");

    // Get all image files in the images directory
    const std::vector img_list{
        ListImageFiles(m_Data.m_ImageDir)
    };

    // Check that we have all our cards accounted for
    for (const auto& img : img_list)
    {
        auto* card{ FindCard(img) };
        if (card == nullptr && img != m_Cfg.m_FallbackName)
        {
            CardAdded(img);
        }
    }

    // And also check we don't have stale cards in here
    const auto stale_images{
        m_Data.m_Cards |
            std::views::filter([](const auto& item)
                               { return !item.m_ExternalPath.has_value() ||
                                        !fs::exists(item.m_ExternalPath.value()); }) |
            std::views::transform(&CardInfo::m_Name) |
            std::views::filter([&](const auto& img)
                               { return !std::ranges::contains(img_list, img); }) |
            std::ranges::to<std::vector>(),
    };
    for (const fs::path& img : stale_images)
    {
        std::erase(m_Data.m_CardsList, img);
        m_Data.m_Previews.erase(img);
        EatCard(img);
    }
}

fs::path Project::GetOutputFolder() const
{
    return m_Data.GetOutputFolder(m_Cfg);
}

fs::path Project::GetBacksideOutputFolder() const
{
    return m_Data.GetBacksideOutputFolder(m_Cfg);
}

bool Project::HasExternalCards() const
{
    return std::ranges::any_of(m_Data.m_Cards,
                               [](const auto& card)
                               { return !card.m_Transient && card.m_ExternalPath.has_value(); });
}

fs::path Project::GetCardImagePath(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        return card->GetSourcePath(m_Data);
    }
    return m_Data.m_ImageDir / card_name;
}

bool Project::IsCardExternal(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        return !card->m_Transient && card->m_ExternalPath.has_value();
    }
    return false;
}

bool Project::HideCard(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        const bool was_visible{ card->m_Hidden == 0 };
        card->m_Hidden++;
        if (was_visible)
        {
            RemoveCardFromList(card_name);
            CardVisibilityChanged(card_name, false);
            return true;
        }
    }
    return false;
}

bool Project::UnhideCard(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        if (card->m_Hidden == 0)
        {
            LogError("Attempting to unhide card {}, but it is already visible...",
                     card_name.string());
            return false;
        }

        card->m_Hidden--;
        const bool visible{ card->m_Hidden == 0 };
        if (visible)
        {
            AppendCardToList(card_name);
            CardVisibilityChanged(card_name, true);
            return true;
        }
    }
    return false;
}

Image::Rotation Project::GetCardRotation(const fs::path& card_name) const
{
    if (const auto* card{ FindCard(card_name) })
    {
        return card->m_Rotation;
    }
    return Image::Rotation::None;
}

bool Project::RotateCardLeft(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        card->m_Rotation = Image::Rotation{
            (std::to_underlying(card->m_Rotation) + 3) % 4
        };
        CardRotationChanged(card_name, card->m_Rotation);
        return true;
    }
    return false;
}

bool Project::RotateCardRight(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        card->m_Rotation = Image::Rotation{
            (std::to_underlying(card->m_Rotation) + 1) % 4
        };
        CardRotationChanged(card_name, card->m_Rotation);
        return true;
    }
    return false;
}

BleedType Project::GetCardBleedType(const fs::path& card_name) const
{
    if (const auto* card{ FindCard(card_name) })
    {
        return card->m_BleedType;
    }
    return BleedType::Infer;
}

bool Project::SetCardBleedType(const fs::path& card_name, BleedType bleed_type)
{
    if (auto* card{ FindCard(card_name) })
    {
        card->m_BleedType = bleed_type;
        CardBleedTypeChanged(card_name, bleed_type);
        return true;
    }
    return false;
}

BadAspectRatioHandling Project::GetCardBadAspectRatioHandling(const fs::path& card_name) const
{
    if (const auto* card{ FindCard(card_name) })
    {
        return card->m_BadAspectRatioHandling;
    }
    return BadAspectRatioHandling::Ignore;
}

bool Project::SetCardBadAspectRatioHandling(const fs::path& card_name, BadAspectRatioHandling ratio_handling)
{
    if (auto* card{ FindCard(card_name) })
    {
        card->m_BadAspectRatioHandling = ratio_handling;
        CardBadAspectRatioHandlingChanged(card_name, ratio_handling);
        return true;
    }
    return false;
}

uint32_t Project::GetCardCount(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        return card->m_Num;
    }
    return 0;
}

uint32_t Project::SetCardCount(const fs::path& card_name, uint32_t num)
{
    if (auto* card{ FindCard(card_name) })
    {
        const bool visible{ card->m_Hidden == 0 };
        if (visible)
        {
            const auto previous_num{ card->m_Num };
            const auto clamped_num{ std::max(std::min(num, 999u), 0u) };
            card->m_Num = clamped_num;

            if (clamped_num > previous_num)
            {
                AppendCardToList(card_name);
            }
            else
            {
                RemoveCardFromList(card_name);
            }

            return clamped_num;
        }
    }
    return 0;
}

uint32_t Project::IncrementCardCount(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        const bool visible{ card->m_Hidden == 0 };
        if (visible)
        {
            ++card->m_Num;
            AppendCardToList(card_name);
            return card->m_Num;
        }
    }

    return 0;
}

uint32_t Project::DecrementCardCount(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        const bool visible{ card->m_Hidden == 0 };
        if (visible)
        {
            --card->m_Num;
            RemoveCardFromList(card_name);
            return card->m_Num;
        }
    }

    return 0;
}

void Project::CardOrderChanged()
{
    std::ranges::sort(m_Data.m_Cards, GetSortFunction(m_Cfg));
}

void Project::CardOrderDirectionChanged()
{
    std::ranges::sort(m_Data.m_Cards, GetSortFunction(m_Cfg));
}

void Project::RestoreCardsOrder()
{
    m_Data.m_CardsList.clear();
}

bool Project::ReorderCards(size_t from, size_t to)
{
    const bool generate_new{ m_Data.m_CardsList.empty() };
    if (generate_new)
    {
        m_Data.m_CardsList = m_Data.GenerateDefaultCardsSorting();
    }

    if (from >= m_Data.m_CardsList.size() ||
        to >= m_Data.m_CardsList.size())
    {
        if (generate_new)
        {
            m_Data.m_CardsList.clear();
        }

        return false;
    }

    const auto from_it{ m_Data.m_CardsList.begin() + from };
    const auto from_card{ *from_it };
    m_Data.m_CardsList.erase(from_it);

    const auto to_it{ m_Data.m_CardsList.begin() + to };
    m_Data.m_CardsList.insert(to_it, from_card);

    return true;
}

CardInfo& Project::CardAdded(const fs::path& card_name)
{
    auto* card{ FindCard(card_name) };
    if (card == nullptr)
    {
        card = &PutCard(card_name);
        for (const auto& other_card : m_Data.m_Cards)
        {
            if (other_card.m_Backside == card_name)
            {
                card->m_Hidden++;
            }
        }
    }
    else if (card->m_Transient)
    {
        --card->m_Hidden;
        card->m_Transient = false;
        card->m_ExternalPath = std::nullopt;

        if (m_Data.m_BacksideEnabled && HasNonClearNonDefaultBackside(*card))
        {
            HideCard(card->m_Backside.value());
        }
    }

    AutoMatchBackside(card_name);
    AppendCardToList(card_name);

    if (m_Data.m_StalePreviews.contains(card_name))
    {
        m_Data.m_Previews[card_name] = std::move(m_Data.m_StalePreviews.at(card_name));
        m_Data.m_StalePreviews.erase(card_name);
        PreviewUpdated(card_name, m_Data.m_Previews.at(card_name));
    }

    return *card;
}

void Project::CardRemoved(const fs::path& card_name)
{
    auto* card{ FindCard(card_name) };
    if (card != nullptr && !card->m_Transient)
    {
        ++card->m_Hidden;
        card->m_Transient = true;

        if (m_Data.m_BacksideEnabled && HasNonClearNonDefaultBackside(*card))
        {
            UnhideCard(card->m_Backside.value());
        }

        if (const auto& frontside{ MatchAsAutoBackside(card_name) })
        {
            SetBacksideImage(frontside.value(), "");

            auto& front_card{ *FindCard(frontside.value()) };
            front_card.m_BacksideAutoAssigned = false;
        }

        if (m_Data.m_Previews.contains(card_name))
        {
            m_Data.m_StalePreviews[card_name] = std::move(m_Data.m_Previews.at(card_name));
            m_Data.m_Previews.erase(card_name);
            PreviewRemoved(card_name);

            if (!m_PendingStalePreviewCleanup)
            {
                m_PendingStalePreviewCleanup = true;
                QTimer::singleShot(
                    0,
                    [this]()
                    {
                        m_Data.m_StalePreviews.clear();
                        m_PendingStalePreviewCleanup = false;
                    });
            }
        }
    }

    RemoveCardFromList(card_name);
}

void Project::CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name)
{
    if (m_Data.m_Previews.contains(old_card_name))
    {
        m_Data.m_Previews[new_card_name] = std::move(m_Data.m_Previews.at(old_card_name));
        m_Data.m_Previews.erase(old_card_name);
    }

    if (auto old_card{ EatCard(old_card_name) })
    {
        old_card.value().m_Name = new_card_name;
        old_card.value().m_LastWriteTime = TryGetLastWriteTime(old_card->GetSourceFolder(m_Data) / new_card_name);
        PutCard(std::move(old_card).value());

        for (auto& other_card : m_Data.m_Cards)
        {
            if (other_card.m_Backside == old_card_name)
            {
                if (other_card.m_BacksideAutoAssigned == true)
                {
                    SetBacksideImage(other_card.m_Name, "");
                    UnhideCard(new_card_name);
                }
                else
                {
                    other_card.m_Backside = new_card_name;
                }
            }
        }
        AutoMatchBackside(new_card_name);
        std::ranges::replace(m_Data.m_CardsList, old_card_name, new_card_name);
    }

    const bool old_hidden{ old_card_name.string().starts_with("__") };
    const bool new_hidden{ new_card_name.string().starts_with("__") };
    if (old_hidden != new_hidden)
    {
        if (old_hidden)
        {
            UnhideCard(new_card_name);
        }
        else
        {
            HideCard(new_card_name);
        }
    }
}

void Project::CardModified(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        card->m_LastWriteTime = TryGetLastWriteTime(card->GetSourcePath(m_Data));
    }
}

bool Project::HasCard(const fs::path& card_name) const
{
    return std::ranges::find(m_Data.m_Cards,
                             card_name,
                             &CardInfo::m_Name) != m_Data.m_Cards.end();
}

const CardInfo* Project::FindCard(const fs::path& card_name) const
{
    return m_Data.FindCard(card_name);
}

CardInfo* Project::FindCard(const fs::path& card_name)
{
    return m_Data.FindCard(card_name);
}

bool Project::HasCardByStem(const fs::path& card_name) const
{
    return std::ranges::find(m_Data.m_Cards,
                             card_name,
                             &CardInfo::Stem) != m_Data.m_Cards.end();
}

const CardInfo* Project::FindCardByStem(const fs::path& card_name) const
{
    auto it{ std::ranges::find(m_Data.m_Cards,
                               card_name,
                               &CardInfo::Stem) };
    return it != m_Data.m_Cards.end() ? &*it : nullptr;
}

CardInfo* Project::FindCardByStem(const fs::path& card_name)
{
    auto it{ std::ranges::find(m_Data.m_Cards,
                               card_name,
                               &CardInfo::Stem) };
    return it != m_Data.m_Cards.end() ? &*it : nullptr;
}

CardInfo& Project::PutCard(const fs::path& card_name)
{
    if (auto* existing_card{ FindCard(card_name) })
    {
        return *existing_card;
    }

    CardInfo new_card{
        .m_Name{ card_name },
        .m_Num = 1,
        .m_Hidden = card_name.string().starts_with("__") ? 1u : 0u,
        .m_LastWriteTime{ TryGetLastWriteTime(m_Data.m_ImageDir / card_name) },
        .m_TimeAdded{ CardInfoClock::now() },
    };

    auto insert_at{ std::ranges::upper_bound(m_Data.m_Cards,
                                             new_card,
                                             GetSortFunction(m_Cfg)) };
    auto new_card_it{
        m_Data.m_Cards
            .insert(insert_at, std::move(new_card))
    };

    return *new_card_it;
}

CardInfo& Project::PutCard(CardInfo card)
{
    if (auto* existing_card{ FindCard(card.m_Name) })
    {
        return *existing_card;
    }

    auto insert_at{
        std::ranges::upper_bound(m_Data.m_Cards,
                                 card.m_Name,
                                 {},
                                 &CardInfo::m_Name)
    };
    auto new_card_it{
        m_Data.m_Cards.insert(insert_at,
                              std::move(card))
    };

    return *new_card_it;
}

std::optional<CardInfo> Project::EatCard(const fs::path& card_name)
{
    auto it{ std::ranges::find(m_Data.m_Cards,
                               card_name,
                               &CardInfo::m_Name) };
    if (it == m_Data.m_Cards.end())
    {
        return std::nullopt;
    }

    std::optional<CardInfo> card{ std::move(*it) };
    m_Data.m_Cards.erase(it);
    return card;
}

bool Project::HasPreview(const fs::path& card_name) const
{
    return m_Data.m_Previews.contains(card_name);
}
bool Project::HasBadAspectRatio(const fs::path& card_name) const
{
    if (m_Data.m_Previews.contains(card_name))
    {
        return m_Data.m_Previews.at(card_name).m_BadAspectRatio;
    }
    return false;
}
bool Project::HasBadRotation(const fs::path& card_name) const
{
    if (m_Data.m_Previews.contains(card_name))
    {
        return m_Data.m_Previews.at(card_name).m_BadRotation;
    }
    return false;
}

const ImagePreview& Project::GetPreview(const fs::path& card_name) const
{
    if (m_Data.m_Previews.contains(card_name))
    {
        return m_Data.m_Previews.at(card_name);
    }
    return m_Data.m_FallbackPreview;
}

const Image& Project::GetCroppedPreview(const fs::path& card_name) const
{
    return GetPreview(card_name).m_CroppedImage;
}
const Image& Project::GetUncroppedPreview(const fs::path& card_name) const
{
    return GetPreview(card_name).m_UncroppedImage;
}

const Image& Project::GetCroppedBacksidePreview(const fs::path& card_name) const
{
    if (const auto backside_image{ GetBacksideImage(card_name) })
    {
        return GetCroppedPreview(backside_image.value().get());
    }
    return m_Data.m_FallbackPreview.m_CroppedImage;
}
const Image& Project::GetUncroppedBacksidePreview(const fs::path& card_name) const
{
    if (const auto backside_image{ GetBacksideImage(card_name) })
    {
        return GetUncroppedPreview(backside_image.value().get());
    }
    return m_Data.m_FallbackPreview.m_UncroppedImage;
}

void Project::SetOutputFilename(fs::path output_filename)
{
    if (m_Data.m_FileName != output_filename)
    {
        m_Data.m_FileName = std::move(output_filename);
        OutputFilenameChanged(m_Data.m_FileName);
    }
}
void Project::SetPageHeaderEnabled(bool page_header_enabled)
{
    if (m_Data.m_RenderPageHeader != page_header_enabled)
    {
        m_Data.m_RenderPageHeader = page_header_enabled;
        PageHeaderEnabledChanged(page_header_enabled);
    }
}

void Project::SetCardSizeChoice(std::string card_size_choice)
{
    if (m_Data.m_CardSizeChoice != card_size_choice &&
        m_Cfg.m_CardSizes.contains(card_size_choice))
    {
        m_Data.m_CardSizeChoice = std::move(card_size_choice);

        CacheCardLayout();

        CardSizeChoiceChanged(m_Data.m_CardSizeChoice);
        CardSizeChanged(ComputeCardsSize());
    }
}
void Project::SetCardOrientation(CardOrientation card_orientation)
{
    if (m_Data.m_CardOrientation != card_orientation)
    {
        m_Data.m_CardOrientation = card_orientation;

        CacheCardLayout();

        CardOrientationChanged(card_orientation);
    }
}
void Project::SetPageSizeChoice(std::string page_size_choice)
{
    if (m_Data.m_PageSize != page_size_choice &&
        m_Cfg.m_PageSizes.contains(page_size_choice))
    {
        m_Data.m_PageSize = std::move(page_size_choice);

        CacheCardLayout();

        PageSizeChoiceChanged(m_Data.m_PageSize);
        PageSizeChanged(ComputePageSize());

        PageMarginsChanged(ComputeMargins());
        MaxPageMarginsChanged(ComputeMaxMargins());
    }
}
void Project::SetBasePdf(std::string base_pdf)
{
    if (m_Data.m_PageSize == Config::c_BasePDFSize &&
        m_Data.m_BasePdf != base_pdf)
    {
        m_Data.m_BasePdf = base_pdf;

        CacheCardLayout();

        BasePdfChanged(m_Data.m_BasePdf);
        PageSizeChanged(ComputePageSize());

        PageMarginsChanged(ComputeMargins());
        MaxPageMarginsChanged(ComputeMaxMargins());
    }
}
void Project::SetPageOrientation(PageOrientation page_orientation)
{
    if (m_Data.m_Orientation != page_orientation)
    {
        m_Data.m_Orientation = page_orientation;
        m_Data.m_CustomMargins.reset();

        CacheCardLayout();

        PageOrientationChanged(page_orientation);
        PageSizeChanged(ComputePageSize());

        PageMarginsChanged(ComputeMargins());
        MaxPageMarginsChanged(ComputeMaxMargins());
    }
}
void Project::SetFlipPageOn(FlipPageOn flip_page_on)
{
    if (m_Data.m_FlipOn != flip_page_on)
    {
        m_Data.m_FlipOn = flip_page_on;
        FlipPageOnChanged(flip_page_on);
    }
}

void Project::SetPageMarginsMode(MarginsMode margins_mode)
{
    if (m_Data.m_MarginsMode != margins_mode)
    {
        const auto previous_margins{ ComputeMargins() };
        const auto previous_max_margins{ ComputeMaxMargins() };

        switch (margins_mode)
        {
        case MarginsMode::Auto:
            m_Data.m_CustomMargins.reset();
            break;
        case MarginsMode::Simple:
            if (!m_Data.m_CustomMargins.has_value())
            {
                m_Data.m_CustomMargins.emplace(CustomMargins{
                    .m_TopLeft{
                        previous_margins.m_Left,
                        previous_margins.m_Top,
                    },
                    .m_BottomRight{ std::nullopt },
                });
            }
            else
            {
                m_Data.m_CustomMargins.value().m_BottomRight.reset();
            }
            break;
        case MarginsMode::Full:
            if (!m_Data.m_CustomMargins.has_value())
            {
                m_Data.m_CustomMargins.emplace(CustomMargins{
                    .m_TopLeft{
                        previous_margins.m_Left,
                        previous_margins.m_Top,
                    },
                    .m_BottomRight{
                        Size{
                            previous_margins.m_Right,
                            previous_margins.m_Bottom,
                        },
                    },
                });
            }
            else if (!m_Data.m_CustomMargins.value().m_BottomRight.has_value())
            {
                m_Data.m_CustomMargins.value().m_BottomRight.emplace(Size{
                    previous_margins.m_Right,
                    previous_margins.m_Bottom,
                });
            }
            break;
        case MarginsMode::Linked:
            m_Data.m_CustomMargins.emplace(CustomMargins{
                .m_TopLeft{
                    previous_margins.m_Left,
                    previous_margins.m_Left,
                },
                .m_BottomRight{
                    Size{
                        previous_margins.m_Left,
                        previous_margins.m_Left,
                    },
                },
            });
            break;
        }

        m_Data.m_MarginsMode = margins_mode;
        PageMarginsModeChanged(margins_mode);

        CacheCardLayout();

        const auto new_margins{ ComputeMargins() };
        if (!RoughlyEqual(previous_margins.m_Left, new_margins.m_Left) ||
            !RoughlyEqual(previous_margins.m_Top, new_margins.m_Top) ||
            !RoughlyEqual(previous_margins.m_Right, new_margins.m_Right) ||
            !RoughlyEqual(previous_margins.m_Bottom, new_margins.m_Bottom))
        {
            PageMarginsChanged(new_margins);
        }

        const auto new_max_margins{ ComputeMaxMargins() };
        if (!RoughlyEqual(previous_max_margins.x, new_max_margins.x) ||
            !RoughlyEqual(previous_max_margins.y, new_max_margins.y))
        {
            MaxPageMarginsChanged(new_max_margins);
        }
    }
}
void Project::SetPageMargin(Margin margin, Length margin_value)
{
    if (m_Data.m_MarginsMode != MarginsMode::Auto &&
        m_Data.m_CustomMargins.has_value())
    {
        auto& custom_margins{ m_Data.m_CustomMargins.value() };
        const auto max_margins{ ComputeMaxMargins() };

        bool has_changed{ false };
        switch (margin)
        {
        case Margin::Top:
            if (!RoughlyEqual(custom_margins.m_TopLeft.y, margin_value))
            {
                custom_margins.m_TopLeft.y = margin_value;
                if (custom_margins.m_BottomRight.has_value() &&
                    custom_margins.m_BottomRight->y + margin_value > max_margins.y)
                {
                    custom_margins.m_BottomRight->y = max_margins.y - margin_value;
                }
                has_changed = true;
            }
            break;
        case Margin::Left:
            if (!RoughlyEqual(custom_margins.m_TopLeft.x, margin_value))
            {
                custom_margins.m_TopLeft.x = margin_value;
                if (custom_margins.m_BottomRight.has_value() &&
                    custom_margins.m_BottomRight->x + margin_value > max_margins.x)
                {
                    custom_margins.m_BottomRight->x = max_margins.x - margin_value;
                }
                has_changed = true;
            }
            break;
        case Margin::Bottom:
            if (custom_margins.m_BottomRight.has_value() &&
                !RoughlyEqual(custom_margins.m_BottomRight->y, margin_value))
            {
                custom_margins.m_BottomRight->y = margin_value;
                if (custom_margins.m_TopLeft.y + margin_value > max_margins.y)
                {
                    custom_margins.m_TopLeft.y = max_margins.y - margin_value;
                }
                has_changed = true;
            }
            break;
        case Margin::Right:
            if (custom_margins.m_BottomRight.has_value() &&
                !RoughlyEqual(custom_margins.m_BottomRight->x, margin_value))
            {
                custom_margins.m_BottomRight->x = margin_value;
                if (custom_margins.m_TopLeft.x + margin_value > max_margins.x)
                {
                    custom_margins.m_TopLeft.x = max_margins.x - margin_value;
                }
                has_changed = true;
            }
            break;
        case Margin::All:
            if (!RoughlyEqual(custom_margins.m_TopLeft.x, margin_value))
            {
                custom_margins.m_TopLeft.x = margin_value;
                custom_margins.m_TopLeft.y = margin_value;
                if (custom_margins.m_BottomRight.has_value())
                {
                    custom_margins.m_BottomRight->x = margin_value;
                    custom_margins.m_BottomRight->y = margin_value;
                }
                has_changed = true;
            }
            break;
        }

        if (has_changed)
        {
            PageMarginsChanged(ComputeMargins());
        }

        CacheCardLayout();
    }
}
void Project::SetCardsLayoutVertical(dla::uvec2 cards_layout)
{
    if (m_Data.m_PageSize == Config::c_FitSize &&
        m_Data.m_CardLayoutVertical != cards_layout)
    {
        m_Data.m_CardLayoutVertical = cards_layout;

        CardsLayoutVerticalChanged(cards_layout);
        CardsSizeChanged(ComputeCardsSize());
    }
}
void Project::SetCardsLayoutHorizontal(dla::uvec2 cards_layout)
{
    if (m_Data.m_PageSize == Config::c_FitSize &&
        m_Data.m_CardLayoutHorizontal != cards_layout)
    {
        m_Data.m_CardLayoutHorizontal = cards_layout;

        CardsLayoutHorizontalChanged(cards_layout);
        CardsSizeChanged(ComputeCardsSize());
    }
}

void Project::SetExportExactGuides(bool export_exact_guides)
{
    if (m_Data.m_ExportExactGuides != export_exact_guides)
    {
        m_Data.m_ExportExactGuides = export_exact_guides;
        ExportExactGuidesChanged(export_exact_guides);
    }
}
void Project::SetGuidesEnabled(bool guides_enabled)
{
    if (m_Data.m_EnableGuides != guides_enabled)
    {
        m_Data.m_EnableGuides = guides_enabled;
        GuidesEnabledChanged(guides_enabled);
    }
}
void Project::SetBacksideGuidesEnabled(bool backside_guides_enabled)
{
    if (m_Data.m_BacksideEnableGuides != backside_guides_enabled)
    {
        m_Data.m_BacksideEnableGuides = backside_guides_enabled;
        BacksideGuidesEnabledChanged(backside_guides_enabled);
    }
}
void Project::SetCornerGuidesEnabled(bool corner_guides_enabled)
{
    if (m_Data.m_CornerGuides != corner_guides_enabled)
    {
        m_Data.m_CornerGuides = corner_guides_enabled;
        CornerGuidesEnabledChanged(corner_guides_enabled);
    }
}
void Project::SetCrossGuidesEnabled(bool cross_guides_enabled)
{
    if (m_Data.m_CrossGuides != cross_guides_enabled)
    {
        m_Data.m_CrossGuides = cross_guides_enabled;
        CrossGuidesEnabledChanged(cross_guides_enabled);
    }
}
void Project::SetExtendedGuidesEnabled(bool extended_guides_enabled)
{
    if (m_Data.m_ExtendedGuides != extended_guides_enabled)
    {
        m_Data.m_ExtendedGuides = extended_guides_enabled;
        ExtendedGuidesEnabledChanged(extended_guides_enabled);
    }
}
void Project::SetGuidesColorA(ColorRGB8 guides_color)
{
    if (m_Data.m_GuidesColorA != guides_color)
    {
        m_Data.m_GuidesColorA = guides_color;
        GuidesColorAChanged(guides_color);
    }
}
void Project::SetGuidesColorB(ColorRGB8 guides_color)
{
    if (m_Data.m_GuidesColorB != guides_color)
    {
        m_Data.m_GuidesColorB = guides_color;
        GuidesColorBChanged(guides_color);
    }
}
void Project::SetGuidesOffset(Length guides_offset)
{
    if (RoughlyEqual(m_Data.m_GuidesOffset, guides_offset))
    {
        return;
    }

    m_Data.m_GuidesOffset = guides_offset;
    GuidesOffsetChanged(guides_offset);
}
void Project::SetGuidesLength(Length guides_length)
{
    if (RoughlyEqual(m_Data.m_GuidesLength, guides_length))
    {
        return;
    }

    m_Data.m_GuidesLength = guides_length;
    GuidesLengthChanged(guides_length);
}
void Project::SetGuidesThickness(Length guides_thickness)
{
    if (RoughlyEqual(m_Data.m_GuidesThickness, guides_thickness))
    {
        return;
    }

    m_Data.m_GuidesThickness = guides_thickness;
    GuidesThicknessChanged(guides_thickness);
}

bool Project::SetBacksideEnabled(bool backside_enabled)
{
    if (m_Data.m_BacksideEnabled != backside_enabled)
    {
        m_Data.m_BacksideEnabled = backside_enabled;

        for (const auto& card : m_Data.m_Cards)
        {
            if (HasNonClearNonDefaultBackside(card))
            {
                if (backside_enabled)
                {
                    HideCard(card.m_Backside.value());
                }
                else
                {
                    UnhideCard(card.m_Backside.value());
                }
            }
        }

        BacksideEnabledChanged(backside_enabled);
        return true;
    }
    return false;
}

void Project::SetSeparateBacksidesEnabled(bool separate_backsides)
{
    if (m_Data.m_SeparateBacksides != separate_backsides)
    {
        m_Data.m_SeparateBacksides = separate_backsides;
        SeparateBacksidesEnabledChanged(separate_backsides);
    }
}

bool Project::HasValidDefaultBackside() const
{
    return !m_Data.m_BacksideDefault.has_value() ||
           fs::exists(GetCardImagePath(m_Data.m_BacksideDefault.value()));
}

void Project::SetBacksideDefault(const fs::path& backside_card_name)
{
    if (m_Data.m_BacksideDefault != backside_card_name)
    {
        if (m_Data.m_BacksideDefault.has_value())
        {
            UnhideCard(m_Data.m_BacksideDefault.value());
        }
        m_Data.m_BacksideDefault = backside_card_name;
        HideCard(m_Data.m_BacksideDefault.value());

        BacksideDefaultChanged(m_Data.m_BacksideDefault);
    }
}

void Project::ClearBacksideDefault()
{
    if (m_Data.m_BacksideDefault.has_value())
    {
        UnhideCard(m_Data.m_BacksideDefault.value());
        m_Data.m_BacksideDefault.reset();

        BacksideDefaultChanged(std::nullopt);
    }
}

void Project::SetBacksideOffset(Size offset)
{
    if (RoughlyEqual(m_Data.m_BacksideOffset, offset))
    {
        return;
    }

    m_Data.m_BacksideOffset = offset;
    BacksideOffsetChanged(offset);
}
void Project::SetBacksideRotation(Angle backside_rotation)
{
    if (RoughlyEqual(m_Data.m_BacksideRotation, backside_rotation))
    {
        return;
    }

    m_Data.m_BacksideRotation = backside_rotation;
    BacksideRotationChanged(backside_rotation);
}
void Project::SetBacksideExtraBleedEdge(Length backside_extra_bleed_edge)
{
    if (RoughlyEqual(m_Data.m_BacksideExtraBleedEdge, backside_extra_bleed_edge))
    {
        return;
    }

    m_Data.m_BacksideExtraBleedEdge = backside_extra_bleed_edge;
    BacksideExtraBleedEdgeChanged(backside_extra_bleed_edge);
    EnsureOutputFolder();
}

bool Project::HasClearBacksideImage(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        return !card->m_Backside.has_value();
    }
    return false;
}

bool Project::HasNonDefaultBacksideImage(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        return !HasDefaultBackside(*card);
    }
    return false;
}

OptionalImageRef Project::GetBacksideImage(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        if (!card->m_Backside.has_value())
        {
            return std::nullopt;
        }
        else if (!card->m_Backside.value().empty())
        {
            return card->m_Backside.value();
        }
    }

    if (m_Data.m_BacksideDefault.has_value())
    {
        return m_Data.m_BacksideDefault.value();
    }
    return std::nullopt;
}
bool Project::SetBacksideImage(const fs::path& card_name, fs::path backside_image)
{
    if (card_name == backside_image)
    {
        return false;
    }

    if (auto* card{ FindCard(card_name) })
    {
        if (card->m_Backside == backside_image)
        {
            return false;
        }

        auto old_backside{ std::move(card->m_Backside) };
        card->m_Backside = std::move(backside_image);

        CardBacksideChanged(card_name, card->m_Backside.value());

        const bool old_backside_shown{ old_backside.has_value() ? UnhideCard(old_backside.value()) : false };
        const bool new_backside_hidden{ HideCard(card->m_Backside.value()) };
        return old_backside_shown || new_backside_hidden;
    }

    return false;
}
bool Project::SetBacksideImageDefault(const fs::path& card_name)
{
    return SetBacksideImage(card_name, "");
}
bool Project::ClearBacksideImage(const fs::path& card_name)
{
    if (auto* card{ FindCard(card_name) })
    {
        if (card->m_Backside.has_value())
        {
            const bool old_backside_shown{ UnhideCard(card->m_Backside.value()) };
            card->m_Backside = std::nullopt;
            CardBacksideChanged(card_name, std::nullopt);
            return old_backside_shown;
        }
    }

    return false;
}

bool Project::HasCardBacksideShortEdge(const fs::path& card_name) const
{
    if (auto* card{ FindCard(card_name) })
    {
        return card->m_BacksideShortEdge;
    }
    return false;
}

void Project::SetCardBacksideShortEdge(const fs::path& card_name, bool has_backside_short_edge)
{
    if (auto* card{ FindCard(card_name) })
    {
        card->m_BacksideShortEdge = has_backside_short_edge;
    }
}

bool Project::SetBacksideAutoPattern(std::string pattern)
{
    const auto placeholder_pos{ pattern.find('$') };
    if (placeholder_pos == std::string::npos)
    {
        return false;
    }

    if (pattern == m_Data.m_BacksideAutoPattern)
    {
        return false;
    }

    m_Data.m_BacksideAutoPattern = std::move(pattern);
    BacksideAutoPatternChanged(m_Data.m_BacksideAutoPattern);

    bool any_backside_change{ false };
    for (auto& card : m_Data.m_Cards)
    {
        if (AutoMatchBackside(card.m_Name))
        {
            any_backside_change = true;
        }
    }

    return any_backside_change;
}

bool Project::CacheCardLayout()
{
    const bool fit_size{ m_Data.m_PageSize == Config::c_FitSize };
    if (fit_size)
    {
        return false;
    }

    // Calculate available space after accounting for margins
    const Size page_size{ ComputePageSize() };
    Size available_space{ page_size };
    if (m_Data.m_MarginsMode == MarginsMode::Full ||
        m_Data.m_MarginsMode == MarginsMode::Linked)
    {
        const auto margins{ ComputeMargins() };
        available_space.x -= (margins.m_Left + margins.m_Right);
        available_space.y -= (margins.m_Top + margins.m_Bottom);
    }

    const auto previous_layout_vertical{ m_Data.m_CardLayoutVertical };
    const auto previous_layout_horizontal{ m_Data.m_CardLayoutHorizontal };

    const auto auto_layout{ m_Data.ComputeAutoCardLayout(m_Cfg, available_space) };
    m_Data.m_CardLayoutVertical = auto_layout.m_CardLayoutVertical;
    m_Data.m_CardLayoutHorizontal = auto_layout.m_CardLayoutHorizontal;

    const bool card_layout_vertical_changed{
        previous_layout_vertical != m_Data.m_CardLayoutVertical
    };
    const bool card_layout_horizontal_changed{
        previous_layout_horizontal != m_Data.m_CardLayoutHorizontal
    };
    const bool card_layout_changed{
        card_layout_vertical_changed ||
        card_layout_horizontal_changed
    };
    if (card_layout_changed)
    {
        m_Data.m_SkippedLayoutSlots.clear();

        if (card_layout_vertical_changed)
        {
            CardsLayoutVerticalChanged(m_Data.m_CardLayoutVertical);
        }
        if (card_layout_horizontal_changed)
        {
            CardsLayoutHorizontalChanged(m_Data.m_CardLayoutHorizontal);
        }
    }

    return card_layout_changed;
}

std::optional<fs::path> Project::GetBasePdfPath() const
{
    return m_Data.GetBasePdfPath();
}

Size Project::ComputePageSize() const
{
    const bool fit_size{ m_Data.m_PageSize == Config::c_FitSize };

    if (fit_size)
    {
        return ComputeCardsSize();
    }
    else if (const auto base_pdf_path{ GetBasePdfPath() })
    {
        return LoadPdfSize(base_pdf_path.value())
            .value_or(m_Cfg.GetFirstValidPageSizeInfo().m_Dimensions);
    }
    else
    {
        auto page_size{
            m_Cfg.m_PageSizes.contains(m_Data.m_PageSize)
                ? m_Cfg.m_PageSizes.at(m_Data.m_PageSize).m_Dimensions
                : m_Cfg.GetFirstValidPageSizeInfo().m_Dimensions
        };
        if (m_Data.m_Orientation == PageOrientation::Landscape)
        {
            std::swap(page_size.x, page_size.y);
        }
        return page_size;
    }
}

Size Project::ComputeExactBordersSize() const
{
    return m_Data.ComputeExactBordersSize(m_Cfg);
}

Size Project::ComputeCardsSize() const
{
    return m_Data.ComputeCardsSize(m_Cfg);
}

Size Project::ComputeCardsSizeVertical() const
{
    if (m_Data.m_CardLayoutVertical.x > 0 && m_Data.m_CardLayoutVertical.y > 0)
    {
        const auto card_size_with_bleed{ CardSizeWithBleed() };
        return m_Data.ComputeCardsSize(card_size_with_bleed, m_Data.m_CardLayoutVertical);
    }

    return {};
}

Size Project::ComputeCardsSizeHorizontal() const
{
    if (m_Data.m_CardLayoutHorizontal.x > 0 && m_Data.m_CardLayoutHorizontal.y > 0)
    {
        const auto card_size_with_bleed{ dla::rotl(CardSizeWithBleed()) };
        return m_Data.ComputeCardsSize(card_size_with_bleed, m_Data.m_CardLayoutHorizontal);
    }

    return {};
}

Margins Project::ComputeMargins() const
{
    return m_Data.ComputeMargins(m_Cfg);
}

Size Project::ComputeMaxMargins() const
{
    return m_Data.ComputeMaxMargins(m_Cfg);
}

Size Project::ComputeDefaultMargins() const
{
    return m_Data.ComputeMaxMargins(m_Cfg);
}

float Project::CardRatio() const
{
    return m_Data.CardRatio(m_Cfg);
}

Size Project::CardSize() const
{
    return m_Data.CardSize(m_Cfg);
}

Size Project::CardSizeWithBleed() const
{
    return m_Data.CardSizeWithBleed(m_Cfg);
}

Size Project::CardSizeWithFullBleed() const
{
    return m_Data.CardSizeWithFullBleed(m_Cfg);
}

Length Project::CardFullBleed() const
{
    return m_Data.CardFullBleed(m_Cfg);
}

bool Project::IsCardRoundedRect() const
{
    return m_Data.IsCardRoundedRect(m_Cfg);
}

Length Project::CardCornerRadius() const
{
    return m_Data.CardCornerRadius(m_Cfg);
}

bool Project::IsCardSvg() const
{
    return m_Data.IsCardSvg(m_Cfg);
}

const Svg& Project::CardSvgData() const
{
    return m_Data.CardSvgData(m_Cfg);
}

void Project::SetImageDir(fs::path new_image_dir)
{
    if (new_image_dir != m_Data.m_ImageDir)
    {
        const auto old_image_dir{ std::move(m_Data.m_ImageDir) };
        SetImageDir(std::move(new_image_dir));
        m_Data.m_ImageDir = std::move(new_image_dir);
        m_Data.m_CropDir = m_Data.m_ImageDir / "crop";
        m_Data.m_UncropDir = m_Data.m_ImageDir / "uncrop";
        m_Data.m_ImageCache = m_Data.m_CropDir / "preview.cache";

        Init();

        ImageDirChanged(old_image_dir, m_Data.m_ImageDir);
        EnsureOutputFolder();
    }
}

void Project::EnsureOutputFolder() const
{
    static constexpr auto c_CreateDirectories{
        [](const auto& path)
        {
            std::error_code error_code;
            if (!fs::create_directories(path, error_code))
            {
                LogError("Failed to create directories: {}", error_code.message());
            }
        }
    };

    {
        const auto output_dir{ GetOutputFolder() };
        if (!fs::exists(output_dir))
        {
            c_CreateDirectories(output_dir);
        }
    }

    {
        const auto output_dir{ GetBacksideOutputFolder() };
        if (!fs::exists(output_dir))
        {
            c_CreateDirectories(output_dir);
        }
    }

    if (!fs::exists(m_Data.m_UncropDir))
    {
        c_CreateDirectories(m_Data.m_UncropDir);
    }
}

void Project::SetBleedEdge(Length bleed_edge)
{
    if (RoughlyEqual(m_Data.m_BleedEdge, bleed_edge))
    {
        return;
    }

    m_Data.m_BleedEdge = bleed_edge;
    BleedEdgeChanged(bleed_edge);
    EnsureOutputFolder();

    CardsSizeChanged(ComputeCardsSize());

    CacheCardLayout();
}
void Project::SetEnvelopeBleedEdge(Length envelope_bleed_edge)
{
    if (RoughlyEqual(m_Data.m_EnvelopeBleedEdge, envelope_bleed_edge))
    {
        return;
    }

    m_Data.m_EnvelopeBleedEdge = envelope_bleed_edge;
    EnvelopeBleedEdgeChanged(envelope_bleed_edge);
    EnsureOutputFolder();

    CardsSizeChanged(ComputeCardsSize());

    CacheCardLayout();
}

void Project::SetSpacing(Size spacing)
{
    if (RoughlyEqual(m_Data.m_Spacing, spacing))
    {
        return;
    }

    m_Data.m_Spacing = spacing;
    SpacingChanged(spacing);

    CardsSizeChanged(ComputeCardsSize());

    CacheCardLayout();
}
void Project::SetSpacingLinked(bool spacing_linked)
{
    if (m_Data.m_SpacingLinked != spacing_linked)
    {
        m_Data.m_SpacingLinked = spacing_linked;
        SpacingLinkedChanged(spacing_linked);

        if (spacing_linked)
        {
            SetSpacing({ m_Data.m_Spacing.x, m_Data.m_Spacing.x });
        }
    }
}

void Project::SetCorners(CardCorners corners)
{
    if (m_Data.m_Corners != corners)
    {
        m_Data.m_Corners = corners;
        CornersChanged(corners);
    }
}

ProjectData::ProjectData(const std::string_view default_card_size_choice,
                         const std::string_view default_page_size_choice)
    : m_CardSizeChoice{ default_card_size_choice }
    , m_PageSize{ default_page_size_choice }
{
}
ProjectData::ProjectData(const Config& config)
    : ProjectData{ config.GetFirstValidCardSize(),
                   config.GetFirstValidPageSize() }
{
}

fs::path ProjectData::GetOutputFolder(const ConfigData& config) const
{
    return GetOutputDir(m_CropDir,
                        m_BleedEdge + m_EnvelopeBleedEdge,
                        config.m_ColorCube);
}

fs::path ProjectData::GetBacksideOutputFolder(const ConfigData& config) const
{
    return GetOutputDir(m_CropDir,
                        m_BleedEdge + m_EnvelopeBleedEdge + m_BacksideExtraBleedEdge,
                        config.m_ColorCube);
}

ProjectData::CardLayout ProjectData::ComputeAutoCardLayout(
    const ConfigData& config,
    Size available_space) const
{
    switch (m_CardOrientation)
    {
    default:
    case CardOrientation::Vertical:
        return {
            .m_CardLayoutVertical{ ComputeCardLayout(config, available_space, CardOrientation::Vertical) },
            .m_CardLayoutHorizontal{},
        };
    case CardOrientation::Horizontal:
        return {
            .m_CardLayoutVertical{},
            .m_CardLayoutHorizontal{ ComputeCardLayout(config, available_space, CardOrientation::Horizontal) },
        };
    case CardOrientation::Mixed:
    {
        const auto card_layout_vertical{ ComputeCardLayout(config, available_space, CardOrientation::Vertical) };
        available_space.y -= ComputeCardsSize(CardSizeWithBleed(config), card_layout_vertical).y + m_Spacing.y;
        return {
            .m_CardLayoutVertical{ card_layout_vertical },
            .m_CardLayoutHorizontal{ ComputeCardLayout(config, available_space, CardOrientation::Horizontal) },
        };
    }
    }
}

dla::uvec2 ProjectData::ComputeCardLayout(const ConfigData& config,
                                          Size available_space,
                                          CardOrientation orientation) const
{
    if (available_space.x <= 0_mm || available_space.y <= 0_mm)
    {
        return {};
    }

    const Size card_size_with_bleed{
        orientation == CardOrientation::Horizontal ? dla::rotl(CardSizeWithBleed(config))
                                                   : CardSizeWithBleed(config)
    };

    auto layout{ static_cast<dla::uvec2>(dla::floor(available_space / card_size_with_bleed)) };

    if (m_EnvelopeBleedEdge > 0_mm || m_Spacing.x > 0_mm || m_Spacing.y > 0_mm)
    {
        const Size cards_size{ ComputeCardsSize(card_size_with_bleed, layout) };
        if (cards_size.x > available_space.x)
        {
            layout.x--;
        }
        if (cards_size.y > available_space.y)
        {
            layout.y--;
        }
    }

    return layout;
}

std::optional<fs::path> ProjectData::GetBasePdfPath() const
{
    const bool infer_size{ m_PageSize == Config::c_BasePDFSize };
    if (infer_size)
    {
        return (m_BasePdfsFolder / m_BasePdf).replace_extension(".pdf");
    }
    return std::nullopt;
}

Size ProjectData::ComputePageSize(const ConfigData& config) const
{
    const bool fit_size{ m_PageSize == Config::c_FitSize };

    if (fit_size)
    {
        return ComputeCardsSize(config);
    }
    else if (const auto base_pdf_path{ GetBasePdfPath() })
    {
        return LoadPdfSize(base_pdf_path.value())
            .value_or(config.GetFirstValidPageSizeInfo().m_Dimensions);
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

Size ProjectData::ComputeExactBordersSize(const ConfigData& config) const
{
    return ComputeCardsSize(config) - m_EnvelopeBleedEdge * 2 - m_BleedEdge * 2;
}

Size ProjectData::ComputeCardsSize(const ConfigData& config) const
{
    const bool has_vertical_layout{ m_CardLayoutVertical.x > 0 && m_CardLayoutVertical.y > 0 };
    const bool has_horizontal_layout{ m_CardLayoutHorizontal.x > 0 && m_CardLayoutHorizontal.y > 0 };
    if (has_vertical_layout && has_horizontal_layout)
    {
        const auto card_size_with_bleed{ CardSizeWithBleed(config) };
        const auto verical_cards_size{ ComputeCardsSize(card_size_with_bleed, m_CardLayoutVertical) };

        const auto card_size_with_bleed_horizontal{ dla::rotl(card_size_with_bleed) };
        const auto horizontal_cards_size{ ComputeCardsSize(card_size_with_bleed_horizontal, m_CardLayoutHorizontal) };

        return Size{
            dla::math::max(verical_cards_size.x, horizontal_cards_size.x),
            verical_cards_size.y + horizontal_cards_size.y + m_Spacing.y,
        };
    }
    else if (has_vertical_layout)
    {
        const auto card_size_with_bleed{ CardSizeWithBleed(config) };
        return ComputeCardsSize(card_size_with_bleed, m_CardLayoutVertical);
    }
    else if (has_horizontal_layout)
    {
        const auto card_size_with_bleed{ dla::rotl(CardSizeWithBleed(config)) };
        return ComputeCardsSize(card_size_with_bleed, m_CardLayoutHorizontal);
    }

    return {};
}

Size ProjectData::ComputeCardsSize(const Size& card_size_with_bleed, const dla::uvec2& card_layout) const
{
    if (card_layout.x == 0 || card_layout.y == 0)
    {
        return {};
    }

    return card_layout * card_size_with_bleed +
           (card_layout - 1) * m_Spacing + m_EnvelopeBleedEdge * 2;
}

Margins ProjectData::ComputeMargins(const ConfigData& config) const
{
    // Custom margins take precedence over computed defaults to allow user-defined layouts
    // for specific printing requirements or aesthetic preferences
    if (m_CustomMargins.has_value())
    {
        const auto& custom_margins{ m_CustomMargins.value() };
        const auto& top_left_margins{ custom_margins.m_TopLeft };
        if (!custom_margins.m_BottomRight.has_value())
        {
            // If only top-left margins are specified by user we essentially treat this as
            // an offset of the cards to one side
            const auto max_margins{ ComputeMaxMargins(config) };
            const auto bottom_right_margins{ max_margins - top_left_margins };
            return Margins{
                .m_Left{ top_left_margins.x },
                .m_Top{ top_left_margins.y },
                .m_Right{ bottom_right_margins.x },
                .m_Bottom{ bottom_right_margins.y },
            };
        }
        else
        {
            // If all margins are specified by user pass these directly
            const auto& bottom_right_margins{ custom_margins.m_BottomRight.value() };
            return Margins{
                .m_Left{ top_left_margins.x },
                .m_Top{ top_left_margins.y },
                .m_Right{ bottom_right_margins.x },
                .m_Bottom{ bottom_right_margins.y },
            };
        }
    }

    // Default to centered margins by dividing available space equally
    // This provides a balanced layout suitable for most printing scenarios
    const auto half_max_margins{ ComputeDefaultMargins(config) };
    return Margins{
        half_max_margins.x,
        half_max_margins.y,
        half_max_margins.x,
        half_max_margins.y,
    };
}

Size ProjectData::ComputeMaxMargins(const ConfigData& config) const
{
    return ComputeMaxMargins(config, m_MarginsMode);
}

Size ProjectData::ComputeMaxMargins(const ConfigData& config, MarginsMode margins_mode) const
{
    const Size page_size{ ComputePageSize(config) };
    const auto card_size_with_bleed{ CardSizeWithBleed(config) };
    switch (margins_mode)
    {
    case MarginsMode::Auto:
        [[fallthrough]];
    case MarginsMode::Simple:
    {
        // We can not rely on a pre-computed layout here, so we compute
        // the best case layout and compute margins from that
        const auto card_layout{ ComputeAutoCardLayout(config, page_size) };
        const auto cards_size_vertical{ ComputeCardsSize(card_size_with_bleed,
                                                         card_layout.m_CardLayoutVertical) };
        const auto cards_size_horizontal{ ComputeCardsSize(dla::rotl(card_size_with_bleed),
                                                           card_layout.m_CardLayoutHorizontal) };
        const Size cards_size{
            dla::math::max(cards_size_vertical.x, cards_size_horizontal.x),
            cards_size_vertical.y +
                cards_size_horizontal.y +
                (card_layout.m_CardLayoutVertical.y != 0 && card_layout.m_CardLayoutHorizontal.y != 0 ? m_Spacing.y : 0_mm)
        };

        // With maximum margins the full card layout is pushed all the
        // way to the edge of the page
        const Size max_margins{ page_size - cards_size };
        return max_margins;
    }
    case MarginsMode::Full:
        // With maximum margins we can fit exactly one card, if possible,
        // that is pushed all the way to the edge of the page
        switch (m_CardOrientation)
        {
        case CardOrientation::Mixed:
            [[fallthrough]];
        case CardOrientation::Vertical:
            return dla::max(0_mm, page_size - card_size_with_bleed);
        case CardOrientation::Horizontal:
            return dla::max(0_mm, page_size - dla::rotl(card_size_with_bleed));
        }

        // Fallthrough, we should not land here unless enum values are invalid
        std::unreachable();
    case MarginsMode::Linked:
        // With maximum margins we can fit exactly one card, if possible,
        // that is centered on the page
        switch (m_CardOrientation)
        {
        case CardOrientation::Mixed:
            [[fallthrough]];
        case CardOrientation::Vertical:
            return dla::max(0_mm, page_size - card_size_with_bleed) / 2;
        case CardOrientation::Horizontal:
            return dla::max(0_mm, page_size - dla::rotl(card_size_with_bleed)) / 2;
        }

        // Fallthrough, we should not land here unless enum values are invalid
        std::unreachable();
    }

    // Fallthrough, we should not land here unless enum values are invalid
    std::unreachable();
}

Size ProjectData::ComputeDefaultMargins(const ConfigData& config) const
{
    switch (m_MarginsMode)
    {
    case MarginsMode::Auto:
        [[fallthrough]];
    case MarginsMode::Simple:
        [[fallthrough]];
    case MarginsMode::Full:
        // Center on page
        return ComputeMaxMargins(config, MarginsMode::Auto) / 2.0f;
    case MarginsMode::Linked:
    {
        // Pick smallest margins needed for centering
        const auto center_margins{ ComputeMaxMargins(config, MarginsMode::Auto) / 2.0f };
        const auto min_center_margins{ dla::math::min(center_margins.x, center_margins.y) };
        return Size{ min_center_margins, min_center_margins };
    }
    }

    // Fallthrough, we should not land here unless enum values are invalid
    std::unreachable();
}

const CardSizeInfo& ProjectData::CardSizeInfo(const ConfigData& config) const
{
    const bool has_valid_card_size{ config.m_CardSizes.contains(m_CardSizeChoice) };
    if (!has_valid_card_size)
    {
        LogError("Project has invalid card size '{}' set, defaulting to '{}'...",
                 m_CardSizeChoice,
                 config.GetFirstValidCardSize());
    }
    return has_valid_card_size
               ? config.m_CardSizes.at(m_CardSizeChoice)
               : config.m_CardSizes.begin()->second;
}

float ProjectData::CardRatio(const ConfigData& config) const
{
    const auto& card_size{ CardSize(config) };
    return card_size.x / card_size.y;
}

inline Size GetCardSize(const CardSizeInfo& card_size_info)
{
    if (card_size_info.m_RoundedRect.has_value())
    {
        return card_size_info.m_RoundedRect.value().m_CardSize.m_Dimensions;
    }
    else if (card_size_info.m_SvgInfo.has_value())
    {
        return card_size_info.m_SvgInfo.value().m_Svg.m_Size;
    }
    else
    {
        LogError("Invalid card size, defaulting to 2.48in x 3.46in");
        return { 2.48_mm, 3.46_mm };
    }
}

Size ProjectData::CardSize(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    return GetCardSize(card_size_info) * card_size_info.m_CardSizeScale;
}

Size ProjectData::CardSizeWithBleed(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    return GetCardSize(card_size_info) * card_size_info.m_CardSizeScale + m_BleedEdge * 2;
}

Size ProjectData::CardSizeWithFullBleed(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    return (GetCardSize(card_size_info) + card_size_info.m_InputBleed.m_Dimension * 2) * card_size_info.m_CardSizeScale;
}

Length ProjectData::CardFullBleed(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    return card_size_info.m_InputBleed.m_Dimension * card_size_info.m_CardSizeScale;
}

bool ProjectData::IsCardRoundedRect(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    return card_size_info.m_RoundedRect.has_value();
}

Length ProjectData::CardCornerRadius(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    if (!card_size_info.m_RoundedRect.has_value())
    {
        return 1_cm;
    }
    return card_size_info.m_RoundedRect.value().m_CornerRadius.m_Dimension * card_size_info.m_CardSizeScale;
}

bool ProjectData::IsCardSvg(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    return card_size_info.m_SvgInfo.has_value();
}

const Svg& ProjectData::CardSvgData(const ConfigData& config) const
{
    const auto& card_size_info{ CardSizeInfo(config) };
    if (!card_size_info.m_SvgInfo.has_value())
    {
        static Svg s_Fallback{};
        return s_Fallback;
    }
    return card_size_info.m_SvgInfo.value().m_Svg;
}

const CardInfo* ProjectData::FindCard(const fs::path& card_name) const
{
    auto it{ std::ranges::find(m_Cards,
                               card_name,
                               &CardInfo::m_Name) };
    return it != m_Cards.end() ? &*it : nullptr;
}

CardInfo* ProjectData::FindCard(const fs::path& card_name)
{
    auto it{ std::ranges::find(m_Cards,
                               card_name,
                               &CardInfo::m_Name) };
    return it != m_Cards.end() ? &*it : nullptr;
}

CardSorting ProjectData::GenerateDefaultCardsSorting() const
{
    CardSorting default_cards_list;
    for (const auto& card : m_Cards)
    {
        if (card.m_Hidden == 0)
        {
            for (uint32_t j = 0; j < card.m_Num; j++)
            {
                default_cards_list.push_back(card.m_Name);
            }
        }
    }
    return default_cards_list;
}

void Project::SetPreview(const fs::path& card_name,
                         ImagePreview preview,
                         Image::Rotation rotation)
{
    if (auto* card{ FindCard(card_name) })
    {
        if (rotation == card->m_Rotation)
        {
            PreviewUpdated(card_name, preview);
            const auto update_visibility{
                preview.m_BadRotation ||
                m_Data.m_Previews[card_name].m_BadRotation
            };
            m_Data.m_Previews[card_name] = std::move(preview);
            if (update_visibility)
            {
                CardVisibilityChanged(card_name, true);
            }
        }
    }
}

void Project::CropperDone()
{
    WritePreviews(m_Data.m_ImageCache, m_Data.m_Previews);
}

bool Project::AddExternalCard(const fs::path& absolute_image_path)
{
    const auto card_name{ absolute_image_path.filename() };
    const auto* existing_card{ FindCard(card_name) };
    if (existing_card != nullptr && !existing_card->m_Transient)
    {
        LogError("Can't add card {} since a card with the same name is already part of the project.",
                 card_name.string());
        FailedAddingExternalCard(absolute_image_path);
        return false;
    }
    else
    {
        auto& card{ CardAdded(card_name) };
        card.m_LastWriteTime = TryGetLastWriteTime(absolute_image_path),
        card.m_ExternalPath = absolute_image_path;
        ExternalCardAdded(absolute_image_path);
        return true;
    }
}

bool Project::RemoveExternalCard(const fs::path& card_name)
{
    if (IsCardExternal(card_name))
    {
        const fs::path absolute_image_path{
            FindCard(card_name)->m_ExternalPath.value()
        };
        CardRemoved(card_name);
        ExternalCardRemoved(absolute_image_path);
        return true;
    }
    return false;
}

void Project::AvailableCardSizesChanged(const CardSizes& card_sizes)
{
    if (!std::ranges::contains(card_sizes | c_CardSizeNames, m_Data.m_CardSizeChoice))
    {
        SetCardSizeChoice(std::string{ m_Cfg.GetFirstValidCardSize() });
    }
}
void Project::AvailablePageSizesChanged(const PageSizes& page_sizes)
{
    if (!std::ranges::contains(page_sizes | c_PageSizeNames, m_Data.m_PageSize))
    {
        SetPageSizeChoice(std::string{ m_Cfg.GetFirstValidPageSize() });
    }
}

void Project::AppendCardToList(const fs::path& card_name)
{
    // Empty list implies auto-sorting
    if (m_Data.m_CardsList.empty())
    {
        return;
    }

    if (auto* card{ FindCard(card_name) })
    {

        if (card->m_Hidden == 0)
        {
            const auto current_count{ std::ranges::count(m_Data.m_CardsList, card_name) };
            for (auto i{ current_count }; i < card->m_Num; ++i)
            {
                m_Data.m_CardsList.push_back(card_name);
            }
        }
    }
}

void Project::RemoveCardFromList(const fs::path& card_name)
{
    // Empty list implies auto-sorting
    if (m_Data.m_CardsList.empty())
    {
        return;
    }

    auto* card{ FindCard(card_name) };
    if (card == nullptr || card->m_Num == 0)
    {
        std::erase(m_Data.m_CardsList, card_name);
    }
    else
    {
        const auto current_count{ std::ranges::count(m_Data.m_CardsList, card_name) };
        const auto to_remove{ current_count - card->m_Num };

        auto removed{ 0 };
        for (auto jt = m_Data.m_CardsList.rbegin(); jt != m_Data.m_CardsList.rend() && removed < to_remove;)
        {
            if (*jt == card_name)
            {
                using iter_t = decltype(jt);
                jt = iter_t{ m_Data.m_CardsList.erase(std::next(jt).base()) };
                ++removed;
            }
            else
            {
                ++jt;
            }
        }
    }
}

bool Project::AutoMatchBackside(const fs::path& card_name)
{
    if (auto frontside{ MatchAsAutoBackside(card_name) })
    {
        if (auto* card{ FindCard(frontside.value()) })
        {
            if (HasDefaultBackside(*card) || card->m_BacksideAutoAssigned)
            {
                SetBacksideImage(frontside.value(), card_name);
                card->m_BacksideAutoAssigned = true;
                return true;
            }
        }
    }
    else if (auto* card{ FindCard(card_name) })
    {
        if (HasDefaultBackside(*card) || card->m_BacksideAutoAssigned)
        {
            if (auto backside{ FindCardAutoBackside(card_name) })
            {
                SetBacksideImage(card_name, backside.value());
                card->m_BacksideAutoAssigned = true;
                return true;
            }
            else if (card->m_BacksideAutoAssigned)
            {
                SetBacksideImageDefault(card_name);
                card->m_BacksideAutoAssigned = false;
                return true;
            }
        }
    }

    return false;
}
std::optional<fs::path> Project::FindCardAutoBackside(const fs::path& card_name) const
{
    if (m_Data.m_BacksideAutoPattern.empty())
    {
        return std::nullopt;
    }

    std::string auto_backside{ m_Data.m_BacksideAutoPattern };
    const auto placeholder_pos{ auto_backside.find('$') };
    auto_backside.replace(placeholder_pos, 1, card_name.stem().string());

    for (auto& card : m_Data.m_Cards)
    {
        if (card.m_Name.stem() == auto_backside)
        {
            return card.m_Name;
        }
    }

    return std::nullopt;
}
std::optional<fs::path> Project::MatchAsAutoBackside(const fs::path& card_name) const
{
    const auto pattern{ std::string_view{ m_Data.m_BacksideAutoPattern } };
    if (pattern.empty())
    {
        return std::nullopt;
    }

    const auto placeholder_pos{ pattern.find('$') };
    const auto front{ pattern.substr(0, placeholder_pos) };
    const auto back{ pattern.substr(placeholder_pos + 1, std::string::npos) };

    const auto name_str{ card_name.stem().string() };
    if (name_str.starts_with(front) && name_str.ends_with(back))
    {
        const auto front_name{
            std::string_view{ name_str }
                .substr(0, name_str.size() - back.size())
                .substr(front.size()),
        };
        for (auto& card : m_Data.m_Cards)
        {
            if (HasDefaultBackside(card) || card.m_BacksideAutoAssigned)
            {
                if (card.m_Name.stem() == front_name)
                {
                    return card.m_Name;
                }
            }
        }
    }

    return std::nullopt;
}
