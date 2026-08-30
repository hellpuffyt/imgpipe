#include <algorithm>
#include <cmath>
#include <vector>

#include "imgpipe/ops.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

namespace {

// Reference, unoptimized full 2D Gaussian convolution -- O(N * radius^2) --
// used only to verify that imgpipe's separable two-pass implementation
// (O(N * radius)) produces the same numerical result.
Image gaussianBlur2DReference(const Image& src, double sigma) {
    const int radius = std::max(1, static_cast<int>(std::ceil(sigma * 3.0)));
    std::vector<double> k1d(static_cast<std::size_t>(2 * radius + 1));
    double sum1d = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double v = std::exp(-(i * i) / (2.0 * sigma * sigma));
        k1d[static_cast<std::size_t>(i + radius)] = v;
        sum1d += v;
    }
    for (double& v : k1d) {
        v /= sum1d;
    }

    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();
    Image dst(w, h, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < channels; ++c) {
                double acc = 0.0;
                for (int ky = -radius; ky <= radius; ++ky) {
                    const int sy = std::clamp(y + ky, 0, h - 1);
                    const double wy = k1d[static_cast<std::size_t>(ky + radius)];
                    for (int kx = -radius; kx <= radius; ++kx) {
                        const int sx = std::clamp(x + kx, 0, w - 1);
                        const double wx = k1d[static_cast<std::size_t>(kx + radius)];
                        acc += wx * wy * static_cast<double>(src.pixel(sx, sy, c));
                    }
                }
                dst.pixel(x, y, c) = static_cast<std::uint8_t>(std::clamp(acc, 0.0, 255.0));
            }
        }
    }
    return dst;
}

} // namespace

IMGPIPE_TEST(gaussian_blur_matches_direct_2d_convolution) {
    // This is the test that proves the separable-pass optimization is
    // correct, not merely fast: the two-pass (1D x, then 1D y) result must
    // match a straightforward full 2D convolution with the outer-product
    // kernel, within integer rounding tolerance.
    Image src = makeCheckerboard(24, 24, 3);
    Image separable = ops::gaussianBlur(src, 2.5);
    Image reference = gaussianBlur2DReference(src, 2.5);

    CHECK(separable.width() == reference.width());
    CHECK(separable.height() == reference.height());
    int mismatches = 0;
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            for (int c = 0; c < src.channels(); ++c) {
                const int a = separable.pixel(x, y, c);
                const int b = reference.pixel(x, y, c);
                if (std::abs(a - b) > 1) {
                    ++mismatches;
                }
            }
        }
    }
    CHECK(mismatches == 0);
}

IMGPIPE_TEST(gaussian_blur_smooths_a_single_bright_pixel) {
    Image src = makeSolid(11, 11, 1, 0);
    src.pixel(5, 5, 0) = 255;
    Image blurred = ops::gaussianBlur(src, 1.5);
    // Center should stay the brightest, but no longer 255 (energy spread out).
    CHECK(blurred.pixel(5, 5, 0) < 255);
    CHECK(blurred.pixel(5, 5, 0) > 0);
    CHECK(blurred.pixel(5, 5, 0) > blurred.pixel(0, 0, 0));
    CHECK(blurred.pixel(5, 5, 0) >= blurred.pixel(4, 5, 0));
}

IMGPIPE_TEST(gaussian_blur_one_by_one_image_survives) {
    Image src(1, 1, 4);
    src.pixel(0, 0, 0) = 10;
    src.pixel(0, 0, 1) = 20;
    src.pixel(0, 0, 2) = 30;
    src.pixel(0, 0, 3) = 40;
    Image blurred = ops::gaussianBlur(src, 3.0);
    CHECK(blurred.width() == 1);
    CHECK(blurred.height() == 1);
    CHECK(blurred.pixel(0, 0, 0) == 10);
    CHECK(blurred.pixel(0, 0, 3) == 40);
}

IMGPIPE_TEST(gaussian_blur_rejects_nonpositive_sigma) {
    Image src = makeSolid(4, 4, 1, 100);
    CHECK_THROWS(ops::gaussianBlur(src, 0.0));
    CHECK_THROWS(ops::gaussianBlur(src, -1.0));
}
