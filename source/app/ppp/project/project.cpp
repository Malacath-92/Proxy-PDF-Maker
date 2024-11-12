#include <ppp/project/project.hpp>

#include <ppp/project/image_ops.hpp>

#include <ppp/version.hpp>

#include <nlohmann/json.hpp>

void Project::Load(const fs::path& json_path, PrintFn print_fn)
{
    *this = Project();

    PPP_LOG("Initializing project...");

    try
    {
        const nlohmann::json json{ nlohmann::json::parse(json_path.string()) };
        if (json.contains("version") && !json["version"].is_string() && json["version"].get_ref<const std::string&>() != JsonFormatVersion())
        {
            ImageDir = json["image_dir"].get<std::string>();
            CropDir = ImageDir / "crop";
            ImageCache = json["img_cache"].get<std::string>();

            for (const nlohmann::json& card_json : json["cards"])
            {
                CardInfo& card{ Cards[card_json["name"]] };
                card.Num = card_json["num"];
                card.Backside = card_json["backside"].get<std::string>();
                card.BacksideShortEdge = card_json["backside_short_edge"];
                card.Oversized = card_json["oversized"];
            }

            BleedEdge.value = json["bleed_edge"];

            BacksideEnabled = json["backside_enabled"];
            BacksideDefault = json["backside_default"].get<std::string>();
            BacksideOffset.value = json["backside_offset"];

            OversizedEnabled = json["oversized_enabled"];

            PageSize = json["pagesize"];
            Orientation = json["orientation"];
            FileName = json["file_name"].get<std::string>();
        }
    }
    catch (std::exception e)
    {
        PPP_LOG("Failed loading project from {}: {}", json_path.string(), e.what());
        PPP_LOG("Continuing with an empty project...", json_path.string(), e.what());
    }

    InitProperties(print_fn);
    InitImages(print_fn);
}

void Project::Dump(const fs::path& json_path, PrintFn print_fn) const
{
    if (std::ofstream file{ json_path })
    {
        PPP_LOG("Writing project to {}...", json_path.string());

        nlohmann::json json{};
        json["version"] = JsonFormatVersion();

        json["image_dir"] = ImageDir.string();
        json["img_cache"] = ImageCache.string();

        std::vector<nlohmann::json> cards;
        for (const auto& [name, card] : Cards)
        {
            nlohmann::json& card_json{ cards.emplace_back() };
            card_json["name"] = name.string();
            card_json["num"] = card.Num;
            card_json["backside"] = card.Backside.string();
            card_json["backside_short_edge"] = card.BacksideShortEdge;
            card_json["oversized"] = card.Oversized;
        }
        json["cards"] = cards;

        json["bleed_edge"] = BleedEdge.value;

        json["backside_enabled"] = BacksideEnabled;
        json["backside_default"] = BacksideDefault.string();
        json["backside_offset"] = BacksideOffset.value;

        json["oversized_enabled"] = OversizedEnabled;

        json["pagesize"] = PageSize;
        json["orientation"] = Orientation;
        json["file_name"] = FileName.string();

        file << json;
        file.close();

        PPP_LOG("Writing preview cache...");
        WritePreviews(ImageCache, Previews);
    }
    else
    {
        PPP_LOG("Failed opening file {} for write...", json_path.string());
    }
}

void Project::InitProperties(PrintFn print_fn)
{
    PPP_LOG("Collecting images...");

    // Get all image files in the crop directory
    const std::vector crop_list{ ListImageFiles(CropDir) };

    // Check that we have all our cards accounted for
    for (const auto& img : crop_list)
    {
        if (!Cards.contains(img))
        {
            Cards[img] = CardInfo{};
            if (img.string().starts_with("__"))
            {
                Cards[img].Num = 0;
            }
        }
    }

    // And also check we don't have stale cards in here
    const auto stale_images{
        Cards |
            std::views::transform([](const auto& item)
                                  { return std::ref(item.first); }) |
            std::views::filter([&](const auto& img)
                               { return !std::ranges::contains(crop_list, img); }) |
            std::ranges::to<std::vector>(),
    };
    for (const fs::path& img : stale_images)
    {
        Cards.erase(img);
    }
}

void Project::InitImages(PrintFn print_fn)
{
    PPP_LOG("Loading preview cache...");
    Previews = ReadPreviews(ImageCache);
}
