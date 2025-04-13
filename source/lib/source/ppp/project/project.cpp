#include <ppp/project/project.hpp>

#include <ranges>

#include <nlohmann/json.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ppp/config.hpp>
#include <ppp/version.hpp>

#include <ppp/pdf/util.hpp>

#include <ppp/project/image_ops.hpp>

Project::~Project()
{
    // Save preview cache, in case we didn't finish generating previews we want some partial work saved
    WritePreviews(Data.ImageCache, Data.Previews);
}

void Project::Load(const fs::path& json_path, PrintFn print_fn)
{
    Data = ProjectData{};

    PPP_LOG("Initializing project...");

    try
    {
        const nlohmann::json json{ nlohmann::json::parse(std::ifstream{ json_path }) };
        if (!json.contains("version") || !json["version"].is_string() || json["version"].get_ref<const std::string&>() != JsonFormatVersion())
        {
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
            card.Oversized = card_json["oversized"];
        }

        Data.BleedEdge.value = json["bleed_edge"];
        Data.CornerWeight = std::clamp(json["corner_weight"].get<float>(), 0.0f, 1.0f);

        Data.BacksideEnabled = json["backside_enabled"];
        Data.BacksideDefault = json["backside_default"].get<std::string>();
        Data.BacksideOffset.value = json["backside_offset"];

        Data.OversizedEnabled = json["oversized_enabled"];

        Data.PageSize = json["pagesize"];
        if (!CFG.PageSizes.contains(Data.PageSize))
        {
            Data.PageSize = CFG.DefaultPageSize;
        }

        Data.BasePdf = json["base_pdf"];
        Data.Margins.x.value = json["margins"]["width"];
        Data.Margins.y.value = json["margins"]["height"];
        Data.CardLayout.x = json["card_layout"]["width"];
        Data.CardLayout.y = json["card_layout"]["height"];
        Data.Orientation = magic_enum::enum_cast<PageOrientation>(json["orientation"].get_ref<const std::string&>())
                               .value_or(PageOrientation::Portrait);
        Data.FileName = json["file_name"].get<std::string>();
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
        PPP_LOG("Failed loading project from {}: {}", json_path.string(), e.what());
        PPP_LOG("Continuing with an empty project...", json_path.string(), e.what());
    }

    Init(print_fn);
}

void Project::Dump(const fs::path& json_path, PrintFn print_fn) const
{
    if (std::ofstream file{ json_path })
    {
        PPP_LOG("Writing project to {}...", json_path.string());

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
            card_json["oversized"] = card.Oversized;
        }
        json["cards"] = cards;

        json["bleed_edge"] = Data.BleedEdge.value;
        json["corner_weight"] = Data.CornerWeight;

        json["backside_enabled"] = Data.BacksideEnabled;
        json["backside_default"] = Data.BacksideDefault.string();
        json["backside_offset"] = Data.BacksideOffset.value;

        json["oversized_enabled"] = Data.OversizedEnabled;

        json["pagesize"] = Data.PageSize;
        json["base_pdf"] = Data.BasePdf;
        json["margins"] = nlohmann::json{
            { "width", Data.Margins.x.value },
            { "height", Data.Margins.y.value },
        };
        json["card_layout"] = nlohmann::json{
            { "width", Data.CardLayout.x },
            { "height", Data.CardLayout.y },
        };
        json["orientation"] = magic_enum::enum_name(Data.Orientation);
        json["file_name"] = Data.FileName.string();
        json["enable_guides"] = Data.EnableGuides;
        json["extended_guides"] = Data.ExtendedGuides;
        json["guides_color_a"] = std::array{ Data.GuidesColorA.r, Data.GuidesColorA.g, Data.GuidesColorA.b };
        json["guides_color_b"] = std::array{ Data.GuidesColorB.r, Data.GuidesColorB.g, Data.GuidesColorB.b };

        file << json;
        file.close();
    }
    else
    {
        PPP_LOG("Failed opening file {} for write...", json_path.string());
    }
}

void Project::Init(PrintFn print_fn)
{
    PPP_LOG("Loading preview cache...");
    Data.Previews = ReadPreviews(Data.ImageCache);

    InitProperties(print_fn);
}

void Project::InitProperties(PrintFn print_fn)
{
    PPP_LOG("Collecting images...");

    // Get all image files in the crop directory or the previews
    const std::vector crop_list{
        CFG.EnableStartupCrop ? ListImageFiles(Data.CropDir)
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
    const Size card_size_with_bleed{ CardSizeWithoutBleed + 2 * Data.BleedEdge };
    return Data.CardLayout * card_size_with_bleed;
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
