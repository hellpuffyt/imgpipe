#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "imgpipe/ops.hpp"

namespace imgpipe::ops {

namespace {

std::uint8_t clampToByte(double v) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
}

std::vector<double> makeGaussianKernel1D(double sigma) {
    // Radius covers +/- 3 sigma, which captures >99.7% of the kernel mass.
    const int radius = std::max(1, static_cast<int>(std::ceil(sigma * 3.0)));
    std::vector<double> kernel(static_cast<std::size_t>(2 * radius + 1));
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double x = static_cast<double>(i);
        const double v = std::exp(-(x * x) / (2.0 * sigma * sigma));
        kernel[static_cast<std::size_t>(i + radius)] = v;
        sum += v;
    }
    for (double& v : kernel) {
        v /= sum;
    }
    return kernel;
}

int clampIndex(int v, int lo, int hi) { return std::clamp(v, lo, hi); }

} // namespace

Image gaussianBlur(const Image& src, double sigma) {
    if (sigma <= 0.0) {
        throw std::invalid_argument("gaussianBlur: sigma must be > 0");
    }
    if (src.empty()) {
        throw std::invalid_argument("gaussianBlur: source image is empty");
    }

    const std::vector<double> kernel = makeGaussianKernel1D(sigma);
    const int radius = static_cast<int>(kernel.size() / 2);
    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();

    // Horizontal pass: src -> temp.
    Image temp(w, h, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < channels; ++c) {
                double acc = 0.0;
                for (int k = -radius; k <= radius; ++k) {
                    const int sx = clampIndex(x + k, 0, w - 1);
                    acc += kernel[static_cast<std::size_t>(k + radius)] *
                           static_cast<double>(src.pixel(sx, y, c));
                }
                temp.pixel(x, y, c) = clampToByte(acc);
            }
        }
    }

    // Vertical pass: temp -> dst.
    Image dst(w, h, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < channels; ++c) {
                double acc = 0.0;
                for (int k = -radius; k <= radius; ++k) {
                    const int sy = clampIndex(y + k, 0, h - 1);
                    acc += kernel[static_cast<std::size_t>(k + radius)] *
                           static_cast<double>(temp.pixel(x, sy, c));
                }
                dst.pixel(x, y, c) = clampToByte(acc);
            }
        }
    }
    return dst;
}

} // namespace imgpipe::ops
