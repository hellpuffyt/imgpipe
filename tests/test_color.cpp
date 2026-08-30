#include "imgpipe/ops.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(grayscale_pure_red_equals_luma_red_weight) {
    Image src = makeSolid(1, 1, 3, 255, 0, 0);
    Image gray = ops::grayscale(src);
    CHECK(gray.channels() == 1);
    CHECK_NEAR(gray.pixel(0, 0, 0), 0.2126 * 255.0, 1.0);
}

IMGPIPE_TEST(grayscale_pure_green_equals_luma_green_weight) {
    Image src = makeSolid(1, 1, 3, 0, 255, 0);
    Image gray = ops::grayscale(src);
    CHECK_NEAR(gray.pixel(0, 0, 0), 0.7152 * 255.0, 1.0);
}

IMGPIPE_TEST(grayscale_pure_blue_equals_luma_blue_weight) {
    Image src = makeSolid(1, 1, 3, 0, 0, 255);
    Image gray = ops::grayscale(src);
    CHECK_NEAR(gray.pixel(0, 0, 0), 0.0722 * 255.0, 1.0);
}

IMGPIPE_TEST(grayscale_not_naive_average) {
    // A naive average would give (255+0+0)/3 = 85. The correct BT.709 luma
    // weighting gives ~54.
    Image src = makeSolid(1, 1, 3, 255, 0, 0);
    Image gray = ops::grayscale(src);
    CHECK(gray.pixel(0, 0, 0) != 85);
    CHECK(gray.pixel(0, 0, 0) < 60);
}

IMGPIPE_TEST(grayscale_preserves_alpha_channel) {
    Image src = makeSolid(2, 2, 4, 100, 150, 200, 42);
    Image gray = ops::grayscale(src);
    CHECK(gray.channels() == 2);
    CHECK(gray.pixel(0, 0, 1) == 42);
}

IMGPIPE_TEST(grayscale_already_gray_is_passthrough) {
    Image src = makeSolid(3, 3, 1, 77);
    Image gray = ops::grayscale(src);
    CHECK(gray.channels() == 1);
    CHECK(gray.pixel(1, 1, 0) == 77);
}

IMGPIPE_TEST(brightness_increases_value_and_clamps) {
    Image src = makeSolid(2, 2, 3, 200, 200, 200);
    Image bright = ops::brightnessContrast(src, 100.0, 1.0);
    CHECK(bright.pixel(0, 0, 0) == 255); // clamps at 255
    Image dim = ops::brightnessContrast(src, -250.0, 1.0);
    CHECK(dim.pixel(0, 0, 0) == 0); // clamps at 0
}

IMGPIPE_TEST(contrast_one_is_identity) {
    Image src = makeSolid(2, 2, 3, 123, 45, 6);
    Image result = ops::brightnessContrast(src, 0.0, 1.0);
    CHECK(result.pixel(0, 0, 0) == 123);
    CHECK(result.pixel(0, 0, 1) == 45);
    CHECK(result.pixel(0, 0, 2) == 6);
}

IMGPIPE_TEST(contrast_pushes_values_away_from_midgrey) {
    Image src(1, 1, 1);
    src.pixel(0, 0, 0) = 200; // above mid-grey (127.5)
    Image result = ops::brightnessContrast(src, 0.0, 2.0);
    // (200 - 127.5) * 2 + 127.5 = 272.5 -> clamps to 255.
    CHECK(result.pixel(0, 0, 0) == 255);
}

IMGPIPE_TEST(brightness_contrast_one_by_one_image_survives) {
    Image src = makeSolid(1, 1, 3, 10, 20, 30);
    Image result = ops::brightnessContrast(src, 5.0, 1.1);
    CHECK(result.width() == 1);
    CHECK(result.height() == 1);
}
