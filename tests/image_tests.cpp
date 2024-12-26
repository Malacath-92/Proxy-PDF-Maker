#include <catch2/catch_test_macros.hpp>

#include <ppp/constants.hpp>
#include <ppp/image.hpp>
#include <ppp/project/image_ops.hpp>

Image g_BaseImage{};

TEST_CASE("Empty image is empty", "[image_empty]")
{
    REQUIRE(g_BaseImage.Width() == 0_pix);
    REQUIRE(g_BaseImage.Height() == 0_pix);
    REQUIRE_THROWS(g_BaseImage.Hash());
}

TEST_CASE("Read image from disk", "[image_read]")
{
    g_BaseImage = Image::Read("fallback.png");
    REQUIRE(g_BaseImage.Width() == 248_pix);
    REQUIRE(g_BaseImage.Height() == 322_pix);
    REQUIRE(g_BaseImage.Hash() == 0x1892b36349d83626);
}

TEST_CASE("Rotate image", "[image_rotate]")
{
    {
        const Image rotated_image{ g_BaseImage.Rotate(Image::Rotation::None) };
        REQUIRE(rotated_image.Width() == g_BaseImage.Width());
        REQUIRE(rotated_image.Height() == g_BaseImage.Height());
        REQUIRE(rotated_image.Hash() == 0x1892b36349d83626);
    }

    {
        const Image rotated_image{ g_BaseImage.Rotate(Image::Rotation::Degree90) };
        REQUIRE(rotated_image.Width() == g_BaseImage.Height());
        REQUIRE(rotated_image.Height() == g_BaseImage.Width());
        REQUIRE(rotated_image.Hash() == 0xc7b6994c26a9d992);
    }

    {
        const Image rotated_image{ g_BaseImage.Rotate(Image::Rotation::Degree180) };
        REQUIRE(rotated_image.Width() == g_BaseImage.Width());
        REQUIRE(rotated_image.Height() == g_BaseImage.Height());
        REQUIRE(rotated_image.Hash() == 0x4d30e6c91c72630c);
    }

    {
        const Image rotated_image{ g_BaseImage.Rotate(Image::Rotation::Degree270) };
        REQUIRE(rotated_image.Width() == g_BaseImage.Height());
        REQUIRE(rotated_image.Height() == g_BaseImage.Width());
        REQUIRE(rotated_image.Hash() == 0x921ccce633038c38);
    }
}

TEST_CASE("Crop image", "[image_crop]")
{
    const Image cropped_image{ g_BaseImage.Crop(15_pix, 15_pix, 15_pix, 15_pix) };
    REQUIRE(g_BaseImage.Width() - cropped_image.Width() == 30_pix);
    REQUIRE(g_BaseImage.Height() - cropped_image.Height() == 30_pix);
    REQUIRE(cropped_image.Hash() == 0x4993c363494836b6);
}

TEST_CASE("Uncrop image", "[image_uncrop]")
{
    const Image uncropped_image{ g_BaseImage.AddBlackBorder(15_pix, 15_pix, 15_pix, 15_pix) };
    REQUIRE(uncropped_image.Width() - g_BaseImage.Width() == 30_pix);
    REQUIRE(uncropped_image.Height() - g_BaseImage.Height() == 30_pix);
    REQUIRE(uncropped_image.Hash() == 0x9c9eb773c9d83623);
}

TEST_CASE("Apply color cube", "[image_color_cube]")
{
    {
        const cv::Mat vibrance_cube{ LoadColorCube("res/vibrance.CUBE") };
        const Image filtered_image{ g_BaseImage.ApplyColorCube(vibrance_cube) };
        REQUIRE(filtered_image.Width() == 248_pix);
        REQUIRE(filtered_image.Height() == 322_pix);

        // color cubes applied differently in debug for better debug performance
#ifdef NDEBUG
        REQUIRE(filtered_image.Hash() == 0x189bb26349d836a7);
#else
        REQUIRE(filtered_image.Hash() == 0x589ab26349d836a7);
#endif
    }

    {
        const cv::Mat madness_cube{ LoadColorCube("tests/madness.CUBE") };
        const Image filtered_image{ g_BaseImage.ApplyColorCube(madness_cube) };
        REQUIRE(filtered_image.Width() == 248_pix);
        REQUIRE(filtered_image.Height() == 322_pix);

        // color cubes applied differently in debug for better debug performance
#ifdef NDEBUG
        REQUIRE(filtered_image.Hash() == 0xe76c4d9cb627c958);
#else
        REQUIRE(filtered_image.Hash() == 0xe76c4d9cb627c958);
#endif
    }
}

TEST_CASE("Shrink image", "[image_resize_shrink]")
{
    const Image resized_image{ g_BaseImage.Resize({ 50_pix, 50_pix }) };
    REQUIRE(resized_image.Width() == 50_pix);
    REQUIRE(resized_image.Height() == 50_pix);
    REQUIRE(resized_image.Hash() == 0x1892b36349d83626);
}

TEST_CASE("Grow image", "[image_resize_grow]")
{
    const Image resized_image{ g_BaseImage.Resize({ 500_pix, 500_pix }) };
    REQUIRE(resized_image.Width() == 500_pix);
    REQUIRE(resized_image.Height() == 500_pix);
    REQUIRE(resized_image.Hash() == 0x1892b36349d83626);
}

TEST_CASE("Calculate DPI", "[image_dpi]")
{
    const auto dpi{ g_BaseImage.Density(CardSizeWithBleed) * 1_in };
    REQUIRE(static_cast<int>(dpi.value) == 87);
}
