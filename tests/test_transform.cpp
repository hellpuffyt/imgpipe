#include "imgpipe/ops.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(crop_extracts_expected_region) {
    Image src = makeGradient(10, 1);
    Image cropped = ops::crop(src, 3, 0, 4, 1);
    CHECK(cropped.width() == 4);
    for (int x = 0; x < 4; ++x) {
        CHECK(cropped.pixel(x, 0, 0) == src.pixel(x + 3, 0, 0));
    }
}

IMGPIPE_TEST(crop_rejects_out_of_bounds) {
    Image src = makeSolid(4, 4, 3, 1);
    CHECK_THROWS(ops::crop(src, 2, 2, 4, 4));
    CHECK_THROWS(ops::crop(src, -1, 0, 2, 2));
    CHECK_THROWS(ops::crop(src, 0, 0, 0, 2));
}

IMGPIPE_TEST(crop_one_by_one_image_survives) {
    Image src = makeSolid(1, 1, 3, 5, 6, 7);
    Image cropped = ops::crop(src, 0, 0, 1, 1);
    CHECK(cropped.pixel(0, 0, 0) == 5);
}

IMGPIPE_TEST(flip_horizontal_reverses_columns) {
    Image src = makeGradient(6, 1);
    Image flipped = ops::flip(src, ops::FlipDirection::Horizontal);
    for (int x = 0; x < 6; ++x) {
        CHECK(flipped.pixel(x, 0, 0) == src.pixel(5 - x, 0, 0));
    }
}

IMGPIPE_TEST(flip_vertical_reverses_rows) {
    Image src(1, 5, 1);
    for (int y = 0; y < 5; ++y) {
        src.pixel(0, y, 0) = static_cast<std::uint8_t>(y * 10);
    }
    Image flipped = ops::flip(src, ops::FlipDirection::Vertical);
    for (int y = 0; y < 5; ++y) {
        CHECK(flipped.pixel(0, y, 0) == src.pixel(0, 4 - y, 0));
    }
}

IMGPIPE_TEST(flip_one_by_one_image_survives) {
    Image src = makeSolid(1, 1, 1, 9);
    Image flipped = ops::flip(src, ops::FlipDirection::Horizontal);
    CHECK(flipped.pixel(0, 0, 0) == 9);
}

IMGPIPE_TEST(rotate90_four_times_returns_original_exactly) {
    Image src = makeGradient(7, 3);
    Image current = src;
    for (int i = 0; i < 4; ++i) {
        current = ops::rotate90(current, 90);
    }
    CHECK(current.width() == src.width());
    CHECK(current.height() == src.height());
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            CHECK(current.pixel(x, y, 0) == src.pixel(x, y, 0));
        }
    }
}

IMGPIPE_TEST(rotate90_swaps_dimensions) {
    Image src(5, 3, 1);
    Image rotated = ops::rotate90(src, 90);
    CHECK(rotated.width() == 3);
    CHECK(rotated.height() == 5);
}

IMGPIPE_TEST(rotate180_matches_two_90_rotations) {
    Image src = makeGradient(6, 4);
    Image direct = ops::rotate90(src, 180);
    Image twice = ops::rotate90(ops::rotate90(src, 90), 90);
    CHECK(direct.width() == twice.width());
    CHECK(direct.height() == twice.height());
    for (int y = 0; y < direct.height(); ++y) {
        for (int x = 0; x < direct.width(); ++x) {
            CHECK(direct.pixel(x, y, 0) == twice.pixel(x, y, 0));
        }
    }
}

IMGPIPE_TEST(rotate90_rejects_non_multiple_of_90) {
    Image src = makeSolid(3, 3, 1, 5);
    CHECK_THROWS(ops::rotate90(src, 45));
}

IMGPIPE_TEST(rotate90_one_by_one_image_survives) {
    Image src = makeSolid(1, 1, 3, 1, 2, 3);
    Image rotated = ops::rotate90(src, 270);
    CHECK(rotated.width() == 1);
    CHECK(rotated.height() == 1);
    CHECK(rotated.pixel(0, 0, 0) == 1);
}
