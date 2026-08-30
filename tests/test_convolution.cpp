#include "imgpipe/ops.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(convolve3x3_identity_kernel_is_passthrough) {
    constexpr std::array<double, 9> identity = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    Image src = makeGradient(8, 8);
    Image result = ops::convolve3x3(src, identity, false);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            CHECK(result.pixel(x, y, 0) == src.pixel(x, y, 0));
        }
    }
}

IMGPIPE_TEST(convolve3x3_box_blur_normalized_smooths_flat_region) {
    Image src = makeSolid(5, 5, 1, 100);
    Image result = ops::convolve3x3(src, ops::kernels::boxBlur3x3, true);
    // A flat image convolved with a normalized box blur stays flat.
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            CHECK(result.pixel(x, y, 0) == 100);
        }
    }
}

IMGPIPE_TEST(convolve3x3_sharpen_increases_center_pixel_contrast) {
    Image src = makeSolid(5, 5, 1, 50);
    src.pixel(2, 2, 0) = 200; // bright spot amid dark surroundings
    Image result = ops::convolve3x3(src, ops::kernels::sharpen, false);
    // Sharpen should push the bright center even higher (clamped) relative to input.
    CHECK(result.pixel(2, 2, 0) >= src.pixel(2, 2, 0));
}

IMGPIPE_TEST(convolve3x3_edge_detect_flat_region_is_near_zero) {
    Image src = makeSolid(5, 5, 1, 128);
    Image result = ops::convolve3x3(src, ops::kernels::edgeDetect, false);
    // Sum of edge-detect kernel is 0, so a flat region should convolve to ~0.
    for (int y = 1; y < 4; ++y) {
        for (int x = 1; x < 4; ++x) {
            CHECK(result.pixel(x, y, 0) == 0);
        }
    }
}

IMGPIPE_TEST(convolve3x3_preserves_alpha) {
    Image src = makeSolid(4, 4, 4, 10, 20, 30, 77);
    Image result = ops::convolve3x3(src, ops::kernels::sharpen, false);
    CHECK(result.pixel(1, 1, 3) == 77);
}

IMGPIPE_TEST(convolve3x3_one_by_one_image_survives) {
    Image src = makeSolid(1, 1, 3, 50, 60, 70);
    Image result = ops::convolve3x3(src, ops::kernels::sharpen, false);
    CHECK(result.width() == 1);
    CHECK(result.height() == 1);
}

IMGPIPE_TEST(convolve3x3_normalize_ignores_zero_sum_kernel) {
    // Normalizing a zero-sum kernel (like edge-detect) must not divide by zero.
    Image src = makeSolid(3, 3, 1, 128);
    Image result = ops::convolve3x3(src, ops::kernels::edgeDetect, true);
    CHECK(result.pixel(1, 1, 0) == 0);
}
