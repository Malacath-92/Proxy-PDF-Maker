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
    WritePreviews(Data.ImageCache, Data.Previews);
}

void Project::Load(const fs::path& json_path)
{
    Data = ProjectData{};

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

        Data.ImageDir = json["image_dir"].get<std::string>();
        Data.CropDir = Data.ImageDir / "crop";
        Data.ImageCache = Data.CropDir / "preview.cache";

        for (const nlohmann::json& card_json : json["cards"])
        {
            CardInfo& card{ Data.Cards[card_json["name"]] };
            card.Num = card_json["num"];
            card.Backside = card_json["backside"].get<std::string>();
            card.BacksideShortEdge = card_json["backside_short_edge"];
        }

        Data.BleedEdge.value = json["bleed_edge"];
        Data.CornerWeight = std::clamp(json["corner_weight"].get<float>(), 0.0f, 1.0f);

        Data.BacksideEnabled = json["backside_enabled"];
        Data.BacksideDefault = json["backside_default"].get<std::string>();
        Data.BacksideOffset.value = json["backside_offset"];

        Data.CardSizeChoice = json["card_size"];
        Data.PageSize = json["page_size"];
        if (!CFG.PageSizes.contains(Data.PageSize))
        {
            Data.PageSize = CFG.DefaultPageSize;
        }

        Data.BasePdf = json["base_pdf"];
        Data.Orientation = magic_enum::enum_cast<PageOrientation>(json["orientation"].get_ref<const std::string&>())
                               .value_or(PageOrientation::Portrait);
        if (Data.PageSize == Config::FitSize)
        {
            Data.CardLayout.x = json["card_layout"]["width"];
            Data.CardLayout.y = json["card_layout"]["height"];
        }
        else
        {
            const Size card_size_with_bleed{ CardSizeWithBleed() };
            Data.CardLayout = static_cast<dla::uvec2>(dla::floor(ComputePageSize() / card_size_with_bleed));
        }

        if (json.contains("custom_margins")) {
            Data.CustomMargins = Size{
                json["custom_margins"]["width"].get<float>() * 1_cm,
                json["custom_margins"]["height"].get<float>() * 1_cm,
            };
        }
        else
        {
            Data.CustomMargins.reset();
        }

        Data.FileName = json["file_name"].get<std::string>();
        Data.ExportExactGuides = json["export_exact_guides"];
        Data.EnableGuides = json["enable_guides"];
        Data.ExtendedGuides = json["extended_guides"];
        Data.GuidesColorA.r = json["guides_color_a"][0];
        Data.GuidesColorA.g = json["guides_color_a"][1];
        Data.GuidesColorA.b = json["guides_color_a"][2];
        Data.GuidesColorB.r = json["guides_color_b"][0];
        Data.GuidesColorB.g = json["guides_color_b"][1];
        Data.GuidesColorB.b = json["guides_color_b"][2];
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

        json["image_dir"] = Data.ImageDir.string();
        json["img_cache"] = Data.ImageCache.string();

        std::vector<nlohmann::json> cards;
        for (const auto& [name, card] : Data.Cards)
        {
            nlohmann::json& card_json{ cards.emplace_back() };
            card_json["name"] = name.string();
            card_json["num"] = card.Num;
            card_json["backside"] = card.Backside.string();
            card_json["backside_short_edge"] = card.BacksideShortEdge;
        }
        json["cards"] = cards;

        json["bleed_edge"] = Data.BleedEdge.value;
        json["corner_weight"] = Data.CornerWeight;

        json["backside_enabled"] = Data.BacksideEnabled;
        json["backside_default"] = Data.BacksideDefault.string();
        json["backside_offset"] = Data.BacksideOffset.value;

        json["card_size"] = Data.CardSizeChoice;
        json["page_size"] = Data.PageSize;
        json["base_pdf"] = Data.BasePdf;
        if (Data.CustomMargins.has_value()) {
            json["custom_margins"] = nlohmann::json{
                { "width", Data.CustomMargins->x / 1_cm },
                { "height", Data.CustomMargins->y / 1_cm },
            };
        }
        json["card_layout"] = nlohmann::json{
            { "width", Data.CardLayout.x },
            { "height", Data.CardLayout.y },
        };
        json["orientation"] = magic_enum::enum_name(Data.Orientation);
        json["file_name"] = Data.FileName.string();
        json["export_exact_guides"] = Data.ExportExactGuides;
        json["enable_guides"] = Data.EnableGuides;
        json["extended_guides"] = Data.ExtendedGuides;
        json["guides_color_a"] = std::array{ Data.GuidesColorA.r, Data.GuidesColorA.g, Data.GuidesColorA.b };
        json["guides_color_b"] = std::array{ Data.GuidesColorB.r, Data.GuidesColorB.g, Data.GuidesColorB.b };

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
    Data.Previews = ReadPreviews(Data.ImageCache);

    InitProperties();
    EnsureOutputFolder();
}

void Project::InitProperties()
{
    LogInfo("Collecting images...");

    // Get all image files in the crop directory or the previews
    const std::vector crop_list{
        CFG.EnableUncrop ? ListImageFiles(Data.ImageDir)
                         : ListImageFiles(Data.ImageDir, Data.CropDir)
    };

    // Check that we have all our cards accounted for
    for (const auto& img : crop_list)
    {
        if (!Data.Cards.contains(img) && img != CFG.FallbackName)
        {
            Data.Cards[img] = CardInfo{};
            if (img.string().starts_with("__"))
            {
                Data.Cards[img].Num = 0;
            }
        }
    }

    // And also check we don't have stale cards in here
    const auto stale_images{
        Data.Cards |
            std::views::transform([](const auto& item)
                                  { return std::ref(item.first); }) |
            std::views::filter([&](const auto& img)
                               { return !std::ranges::contains(crop_list, img); }) |
            std::ranges::to<std::vector>(),
    };
    for (const fs::path& img : stale_images)
    {
        Data.Cards.erase(img);
    }
}

void Project::CardAdded(const fs::path& card_name)
{
    if (!Data.Cards.contains(card_name) && !card_name.string().starts_with("__"))
    {
        Data.Cards[card_name] = CardInfo{ 1 };
    }
}

void Project::CardRemoved(const fs::path& card_name)
{
    Data.Cards.erase(card_name);
    Data.Previews.erase(card_name);
}

void Project::CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name)
{
    if (Data.Cards.contains(old_card_name))
    {
        Data.Cards[new_card_name] = std::move(Data.Cards.at(old_card_name));
        Data.Cards.erase(old_card_name);
    }

    if (Data.Previews.contains(old_card_name))
    {
        Data.Previews[new_card_name] = std::move(Data.Previews.at(old_card_name));
        Data.Previews.erase(old_card_name);
    }
}

bool Project::HasPreview(const fs::path& image_name) const
{
    return Data.Previews.contains(image_name);
}

const Image& Project::GetCroppedPreview(const fs::path& image_name) const
{
    if (Data.Previews.contains(image_name))
    {
        return Data.Previews.at(image_name).CroppedImage;
    }
    return Data.FallbackPreview.CroppedImage;
}
const Image& Project::GetUncroppedPreview(const fs::path& image_name) const
{
    if (Data.Previews.contains(image_name))
    {
        return Data.Previews.at(image_name).UncroppedImage;
    }
    return Data.FallbackPreview.UncroppedImage;
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
    if (Data.Cards.contains(image_name))
    {
        const CardInfo& card{ Data.Cards.at(image_name) };
        if (!card.Backside.empty())
        {
            return card.Backside;
        }
    }
    return Data.BacksideDefault;
}
Size Project::ComputePageSize() const
{
    const bool fit_size{ Data.PageSize == Config::FitSize };
    const bool infer_size{ Data.PageSize == Config::BasePDFSize };

    if (fit_size)
    {
        return ComputeCardsSize();
    }
    else if (infer_size)
    {
        return LoadPdfSize(Data.BasePdf + ".pdf")
            .value_or(CFG.PageSizes["A4"].Dimensions);
    }
    else
    {
        auto page_size{ CFG.PageSizes[Data.PageSize].Dimensions };
        if (Data.Orientation == PageOrientation::Landscape)
        {
            std::swap(page_size.x, page_size.y);
        }
        return page_size;
    }
}

Size Project::ComputeCardsSize() const
{
    const Size card_size_with_bleed{ CardSize() + 2 * Data.BleedEdge };
    return Data.CardLayout * card_size_with_bleed;
}

Size Project::ComputeMargins() const
{
    return Data.ComputeMargins(CFG);
}

float Project::CardRatio() const
{
    return Data.CardRatio(CFG);
}

Size Project::CardSize() const
{
    return Data.CardSize(CFG);
}

Size Project::CardSizeWithBleed() const
{
    return Data.CardSizeWithBleed(CFG);
}

Size Project::CardSizeWithFullBleed() const
{
    return Data.CardSizeWithFullBleed(CFG);
}

Length Project::CardFullBleed() const
{
    return Data.CardFullBleed(CFG);
}

Length Project::CardCornerRadius() const
{
    return Data.CardCornerRadius(CFG);
}

void Project::EnsureOutputFolder() const
{
    const auto output_dir{
        GetOutputDir(Data.CropDir, Data.BleedEdge, CFG.ColorCube)
    };
    if (!fs::exists(output_dir))
    {
        fs::create_directories(output_dir);
    }
}

Size Project::ProjectData::ComputePageSize(const Config& config) const
{
    const bool fit_size{ PageSize == Config::FitSize };
    const bool infer_size{ PageSize == Config::BasePDFSize };

    if (fit_size)
    {
        return ComputeCardsSize(config);
    }
    else if (infer_size)
    {
        return LoadPdfSize(BasePdf + ".pdf")
            .value_or(config.PageSizes.at("A4").Dimensions);
    }
    else
    {
        auto page_size{ config.PageSizes.at(PageSize).Dimensions };
        if (Orientation == PageOrientation::Landscape)
        {
            std::swap(page_size.x, page_size.y);
        }
        return page_size;
    }
}

Size Project::ProjectData::ComputeCardsSize(const Config& config) const
{
    const Size card_size_with_bleed{ CardSize(config) + 2 * BleedEdge };
    return CardLayout * card_size_with_bleed;
}

Size Project::ProjectData::ComputeMargins(const Config& config) const
{
    if (CustomMargins.has_value())
    {
        return CustomMargins.value();
    }

    const Size page_size{ ComputePageSize(config) };
    const Size cards_size{ ComputeCardsSize(config) };
    const Size max_margins{ page_size - cards_size };
    return max_margins / 2.0f;
}

float Project::ProjectData::CardRatio(const Config& config) const
{
    const auto& card_size{ CardSize(config) };
    return card_size.x / card_size.y;
}

Size Project::ProjectData::CardSize(const Config& config) const
{
    const auto& card_size_info{
        config.CardSizes.contains(CardSizeChoice)
            ? config.CardSizes.at(CardSizeChoice)
            : config.CardSizes.at(CFG.DefaultCardSize),
    };
    return card_size_info.CardSize.Dimensions * card_size_info.CardSizeScale;
}

Size Project::ProjectData::CardSizeWithBleed(const Config& config) const
{
    const auto& card_size_info{
        config.CardSizes.contains(CardSizeChoice)
            ? config.CardSizes.at(CardSizeChoice)
            : config.CardSizes.at(config.DefaultCardSize),
    };
    return card_size_info.CardSize.Dimensions * card_size_info.CardSizeScale + BleedEdge * 2;
}

Size Project::ProjectData::CardSizeWithFullBleed(const Config& config) const
{
    const auto& card_size_info{
        config.CardSizes.contains(CardSizeChoice)
            ? config.CardSizes.at(CardSizeChoice)
            : config.CardSizes.at(config.DefaultCardSize),
    };
    return (card_size_info.CardSize.Dimensions + card_size_info.InputBleed.Dimension * 2) * card_size_info.CardSizeScale;
}

Length Project::ProjectData::CardFullBleed(const Config& config) const
{
    const auto& card_size_info{
        config.CardSizes.contains(CardSizeChoice)
            ? config.CardSizes.at(CardSizeChoice)
            : config.CardSizes.at(config.DefaultCardSize),
    };
    return card_size_info.InputBleed.Dimension * card_size_info.CardSizeScale;
}

Length Project::ProjectData::CardCornerRadius(const Config& config) const
{
    const auto& card_size_info{
        config.CardSizes.contains(CardSizeChoice)
            ? config.CardSizes.at(CardSizeChoice)
            : config.CardSizes.at(config.DefaultCardSize),
    };
    return card_size_info.CornerRadius.Dimension * card_size_info.CardSizeScale;
}

void Project::SetPreview(const fs::path& image_name, ImagePreview preview)
{
    PreviewUpdated(image_name, preview);
    Data.Previews[image_name] = std::move(preview);
}

void Project::CropperDone()
{
    WritePreviews(Data.ImageCache, Data.Previews);
}
