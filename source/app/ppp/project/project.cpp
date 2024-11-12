#include <ppp/project/project.hpp>

#include <ppp/project/image_ops.hpp>

#include <ppp/version.hpp>

#include <nlohmann/json.hpp>

void Project::Load(const fs::path& json_path, PrintFn print_fn)
{
    *this = Project();

    if (fs::exists(json_path))
    {
        PPP_LOG("Initializing project...");

        const nlohmann::json json{ nlohmann::json::parse(json_path.string()) };
        if (json.contains("version") && !json["version"].is_string() && json["version"].get_ref<const std::string&>() != JsonFormatVersion())
        {
            ImageDir = json["image_dir"].get<std::string>();
            CropDir = ImageDir / "crop";
            ImageCache = json["img_cache"].get<std::string>();

            for (const nlohmann::json& card_json : json["cards"])
            {
                CardInfo card{};
                card.Num = card_json["num"];
                card.Backside = card_json["backside"].get<std::string>();
                card.BacksideShortEdge = card_json["short_edge"];
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

    InitProperties(print_fn);
    InitImages(print_fn);
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
