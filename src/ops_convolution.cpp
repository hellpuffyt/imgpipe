#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "imgpipe/ops.hpp"

namespace imgpipe::ops {

namespace {
std::uint8_t clampToByte(double v) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
}
int clampIndex(int v, int lo, int hi) { return std::clamp(v, lo, hi); }
} // namespace

Image convolve3x3(const Image& src, const std::array<double, 9>& kernel, bool normalize) {
    if (src.empty()) {
        throw std::invalid_argument("convolve3x3: source image is empty");
    }

    std::array<double, 9> k = kernel;
    if (normalize) {
        double sum = 0.0;
        for (double v : k) {
            sum += v;
        }
        if (sum != 0.0) {
            for (double& v : k) {
                v /= sum;
            }
        }
    }

    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();
    const bool hasAlpha = src.hasAlpha();
    const int colorChannels = hasAlpha ? channels - 1 : channels;

    Image dst(w, h, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < colorChannels; ++c) {
                double acc = 0.0;
                for (int ky = -1; ky <= 1; ++ky) {
                    const int sy = clampIndex(y + ky, 0, h - 1);
                    for (int kx = -1; kx <= 1; ++kx) {
                        const int sx = clampIndex(x + kx, 0, w - 1);
                        const double weight = k[static_cast<std::size_t>((ky + 1) * 3 + (kx + 1))];
                        acc += weight * static_cast<double>(src.pixel(sx, sy, c));
                    }
                }
                dst.pixel(x, y, c) = clampToByte(acc);
            }
            if (hasAlpha) {
                dst.pixel(x, y, channels - 1) = src.pixel(x, y, channels - 1);
            }
        }
    }
    return dst;
}

} // namespace imgpipe::ops
