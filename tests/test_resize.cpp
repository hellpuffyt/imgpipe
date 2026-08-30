#include <algorithm>
#include <cmath>

#include "imgpipe/ops.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(resize_gradient_half_bilinear_known_values) {
    // 5-pixel gradient 0,63,127,191,255 (rounded), halved to a 2-pixel row.
    Image src(5, 1, 1);
    const std::uint8_t values[5] = {0, 63, 127, 191, 255};
    for (int x = 0; x < 5; ++x) {
        src.pixel(x, 0, 0) = values[x];
    }
    Image dst = ops::resize(src, 2, 1, ops::ResizeMethod::Bilinear);
    CHECK(dst.width() == 2);
    CHECK(dst.height() == 1);
    // scaleX = 5/2 = 2.5. dx=0: sx=(0.5*2.5)-0.5=0.75 -> between px0(0) and px1(63), fx=0.75
    // dx=1: sx=(1.5*2.5)-0.5=3.25 -> between px3(191) and px4(255), fx=0.25
    const double expected0 = 0 + (63 - 0) * 0.75;
    const double expected1 = 191 + (255 - 191) * 0.25;
    CHECK_NEAR(dst.pixel(0, 0, 0), expected0, 1.0);
    CHECK_NEAR(dst.pixel(1, 0, 0), expected1, 1.0);
}

IMGPIPE_TEST(resize_upscale_bilinear_interpolates_smoothly) {
    Image src(2, 1, 1);
    src.pixel(0, 0, 0) = 0;
    src.pixel(1, 0, 0) = 100;
    Image dst = ops::resize(src, 4, 1, ops::ResizeMethod::Bilinear);
    CHECK(dst.width() == 4);
    // Values should be non-decreasing across the upscaled row (monotonic gradient).
    for (int x = 1; x < 4; ++x) {
        CHECK(dst.pixel(x, 0, 0) >= dst.pixel(x - 1, 0, 0));
    }
    CHECK(dst.pixel(0, 0, 0) <= 40);
    CHECK(dst.pixel(3, 0, 0) >= 60);
}

IMGPIPE_TEST(resize_box_downscale_checkerboard_averages_to_midgrey) {
    // A 4x4 1px checkerboard downscaled by exactly 2x: each destination
    // pixel covers exactly one 2x2 source block containing two 0s and two
    // 255s, so the box (area) average must be exactly 127.5 (rounds to
    // 128 as an integer byte).
    Image src = makeCheckerboard(4, 4);
    Image dst = ops::resize(src, 2, 2, ops::ResizeMethod::Box);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            CHECK(dst.pixel(x, y, 0) == 128);
        }
    }
}

IMGPIPE_TEST(resize_box_vs_bilinear_aliasing_on_checkerboard) {
    // A finer checkerboard downscaled by a non-integer factor. Box (area)
    // sampling averages every contributing source pixel and stays close to
    // the true mid-grey regardless of sub-pixel alignment. Bilinear only
    // samples a 2x2 neighbourhood, so at off-center fractional positions it
    // can land almost entirely on one checker color and deviate sharply
    // from mid-grey -- this is exactly the aliasing bilinear downscaling
    // produces and box avoids.
    Image src = makeCheckerboard(16, 16);
    Image boxResult = ops::resize(src, 3, 3, ops::ResizeMethod::Box);
    Image bilinearResult = ops::resize(src, 3, 3, ops::ResizeMethod::Bilinear);

    double maxBoxDeviation = 0.0;
    double maxBilinearDeviation = 0.0;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            maxBoxDeviation =
                std::max(maxBoxDeviation, std::abs(static_cast<double>(boxResult.pixel(x, y, 0)) - 127.5));
            maxBilinearDeviation = std::max(
                maxBilinearDeviation, std::abs(static_cast<double>(bilinearResult.pixel(x, y, 0)) - 127.5));
        }
    }
    // Box should stay reasonably close to mid-grey everywhere...
    CHECK(maxBoxDeviation < 40.0);
    // ...while bilinear, sampling a sparse neighbourhood, deviates more.
    CHECK(maxBilinearDeviation > maxBoxDeviation);
}

IMGPIPE_TEST(resize_one_by_one_image_survives) {
    Image src(1, 1, 3);
    src.pixel(0, 0, 0) = 10;
    src.pixel(0, 0, 1) = 20;
    src.pixel(0, 0, 2) = 30;

    Image upBil = ops::resize(src, 5, 5, ops::ResizeMethod::Bilinear);
    Image upBox = ops::resize(src, 5, 5, ops::ResizeMethod::Box);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            CHECK(upBil.pixel(x, y, 0) == 10);
            CHECK(upBox.pixel(x, y, 0) == 10);
        }
    }

    Image downToOne = ops::resize(upBox, 1, 1, ops::ResizeMethod::Box);
    CHECK(downToOne.pixel(0, 0, 0) == 10);
    CHECK(downToOne.pixel(0, 0, 1) == 20);
    CHECK(downToOne.pixel(0, 0, 2) == 30);
}

IMGPIPE_TEST(resize_extreme_aspect_ratio) {
    Image src = makeGradient(100, 1);
    Image dst = ops::resize(src, 20, 50, ops::ResizeMethod::Box);
    CHECK(dst.width() == 20);
    CHECK(dst.height() == 50);
    // A 1-row source stretched to 50 rows should be constant per column (up
    // to +/-1 from independent per-row floating-point rounding).
    for (int x = 0; x < 20; ++x) {
        const std::uint8_t v0 = dst.pixel(x, 0, 0);
        for (int y = 1; y < 50; ++y) {
            CHECK_NEAR(dst.pixel(x, y, 0), v0, 1.0);
        }
    }
}

IMGPIPE_TEST(resize_rejects_nonpositive_dimensions) {
    Image src = makeSolid(4, 4, 3, 100);
    CHECK_THROWS(ops::resize(src, 0, 4, ops::ResizeMethod::Box));
    CHECK_THROWS(ops::resize(src, 4, -1, ops::ResizeMethod::Bilinear));
}
