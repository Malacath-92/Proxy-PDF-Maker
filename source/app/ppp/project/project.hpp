#pragma once

#include <cstdint>
#include <unordered_map>

#include <ppp/config.hpp>
#include <ppp/constants.hpp>
#include <ppp/image.hpp>
#include <ppp/util.hpp>

struct CardInfo
{
    uint32_t Num{ 1 };
    fs::path Backside{};
    bool BacksideShortEdge{ false };
    bool Oversized{ false };
};
using CardMap = std::unordered_map<fs::path, CardInfo>;

struct ImagePreview
{
    Image CroppedImage;
    Image UncroppedImage;
    Image CroppedThumbImage;
};
using ImgDict = std::unordered_map<fs::path, ImagePreview>;

struct Project
{
    void Load(const fs::path& json_path, PrintFn print_fn);
    void Dump(const fs::path& json_path, PrintFn print_fn) const;

    void InitProperties(PrintFn print_fn);
    void InitImages(PrintFn print_fn);

    // Project options
    fs::path ImageDir{ "images" };
    fs::path CropDir{ "images/crop" };
    fs::path ImageCache{ "img.cache" };

    // List of all cards
    CardMap Cards{};
    ImgDict Previews{};

    // Bleed edge options
    Length BleedEdge{ 0_mm };

    // Backside options
    bool BacksideEnabled{ false };
    fs::path BacksideDefault{ "__back.png" };
    Length BacksideOffset{ 0_mm };

    // Oversized options
    bool OversizedEnabled{ false };

    // PDF generation options
    std::string PageSize{ CFG.DefaultPageSize };
    std::string Orientation{ "Portrait" };
    fs::path FileName{ "_printme" };
};

// def init_dict(print_dict, img_dict):
//     default_page_size = CFG.DefaultPageSize
//     default_print_dict = {
//         # project options
//         "image_dir": "images",
//         "img_cache": "img.cache",
//         # list of all cards
//         "cards": {},
//         # backside options
//         "backside_enabled": False,
//         "backside_default": "__back.png",
//         "backside_offset": "0",
//         "backsides": {},
//         "backside_short_edge": {},
//         # oversized options
//         "oversized_enabled": False,
//         "oversized": {},
//         # pdf generation options
//         "pagesize": (
//             default_page_size if default_page_size in page_sizes else "Letter"
//         ),
//         "extended_guides": True,
//         "orient": "Portrait",
//         "bleed_edge": "0",
//         "filename": "_printme",
//     }
//
//     # Initialize our default values
//     for key, value in default_print_dict.items():
//         if key not in print_dict:
//             print_dict[key] = value
//
//     # Get project folders
//     image_dir = print_dict["image_dir"]
//     crop_dir = os.path.join(image_dir, "crop")
//     image.init_image_folder(image_dir, crop_dir)
//
//     # Get all image files in the crop directory
//     crop_list = image.list_image_files(crop_dir)
//
//     # Check that we have all our cards accounted for
//     for img in crop_list:
//         if img not in print_dict["cards"].keys():
//             print_dict["cards"][img] = 0 if img.startswith("__") else 1
//
//     # And also check we don't have stale cards in here
//     stale_images = []
//     for img in print_dict["cards"].keys():
//         if img not in crop_list:
//             stale_images.append(img)
//     for img in stale_images:
//         del print_dict["cards"][img]
//         if img in print_dict["backsides"]:
//             del print_dict["backsides"][img]
//         if img in print_dict["backside_short_edge"]:
//             del print_dict["backside_short_edge"][img]
//         if img in print_dict["oversized"]:
//             del print_dict["oversized"][img]
//
//     # Make sure we have a sensible bleed edge
//     bleed_edge = str(print_dict["bleed_edge"])
//     bleed_edge = util.cap_bleed_edge_str(bleed_edge)
//     if not util.is_number_string(bleed_edge):
//         bleed_edge = "0"
//     print_dict["bleed_edge"] = bleed_edge
//
//     # Initialize the image amount
//     for img in crop_list:
//         if img not in print_dict["cards"].keys():
//             print_dict["cards"][img] = 1
//
//     # Deselect images starting with __
//     for img in crop_list:
//         print_dict["cards"][img] = (
//             0 if img.startswith("__") else print_dict["cards"][img]
//         )
//
//     # Initialize image cache
//     img_cache = print_dict["img_cache"]
//     if os.path.exists(img_cache):
//         with open(img_cache, "r") as fp:
//             loaded_img_dict = json.load(fp)
//             img_dict.clear()
//             for key, value in loaded_img_dict.items():
//                 img_dict[key] = value
//
//
// def init_images(print_dict, img_dict, print_fn):
//     image_dir = print_dict["image_dir"]
//     crop_dir = os.path.join(image_dir, "crop")
//     img_cache = print_dict["img_cache"]
//
//     # setup crops
//     bleed_edge = float(print_dict["bleed_edge"])
//     if image.need_run_cropper(image_dir, crop_dir, bleed_edge, CFG.VibranceBump):
//         image.cropper(
//             image_dir,
//             crop_dir,
//             img_cache,
//             img_dict,
//             bleed_edge,
//             CFG.MaxDPI,
//             CFG.VibranceBump,
//             CFG.EnableUncrop,
//             print_fn,
//         )
//
//     # setup image previews
//     img_cache = print_dict["img_cache"]
//     if image.need_cache_previews(crop_dir, img_dict):
//         image.cache_previews(img_cache, image_dir, crop_dir, print_fn, img_dict)
//
//
// def load(print_dict, img_dict, json_path, print_fn):
//     try:
//         with open(json_path, "r") as fp:
//             loaded_print_dict = json.load(fp)
//             for key, value in loaded_print_dict.items():
//                 print_dict[key] = value
//     except:
//         print_fn("Error: Failed loading project... Resetting...")
//         time.sleep(1)
//         print_dict.clear()
//
//     init_dict(print_dict, img_dict)
//     init_images(print_dict, img_dict, print_fn)
