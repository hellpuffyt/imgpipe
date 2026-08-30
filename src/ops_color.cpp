#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "imgpipe/ops.hpp"

namespace imgpipe::ops {

namespace {
std::uint8_t clampToByte(double v) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
}
} // namespace

Image grayscale(const Image& src) {
    if (src.empty()) {
        throw std::invalid_argument("grayscale: source image is empty");
    }
    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();

    // ITU-R BT.709 luma coefficients -- not a naive (R+G+B)/3 average, which
    // misrepresents perceived brightness (green dominates human luminance
    // perception, blue contributes very little).
    constexpr double kRed = 0.2126;
    constexpr double kGreen = 0.7152;
    constexpr double kBlue = 0.0722;

    if (channels == 1 || channels == 2) {
        // Already gray (with or without alpha): nothing to do.
        return src;
    }

    const bool hasAlpha = src.hasAlpha();
    const int outChannels = hasAlpha ? 2 : 1;
    Image dst(w, h, outChannels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double r = src.pixel(x, y, 0);
            const double g = src.pixel(x, y, 1);
            const double b = src.pixel(x, y, 2);
            const double luma = kRed * r + kGreen * g + kBlue * b;
            dst.pixel(x, y, 0) = clampToByte(luma);
            if (hasAlpha) {
                dst.pixel(x, y, 1) = src.pixel(x, y, 3);
            }
        }
    }
    return dst;
}

Image brightnessContrast(const Image& src, double brightness, double contrast) {
    if (src.empty()) {
        throw std::invalid_argument("brightnessContrast: source image is empty");
    }
    if (contrast < 0.0) {
        throw std::invalid_argument("brightnessContrast: contrast must be >= 0");
    }
    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();
    const bool hasAlpha = src.hasAlpha();
    const int colorChannels = hasAlpha ? channels - 1 : channels;

    Image dst(w, h, channels);
    constexpr double kPivot = 127.5;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < colorChannels; ++c) {
                const double v = static_cast<double>(src.pixel(x, y, c));
                const double adjusted = (v - kPivot) * contrast + kPivot + brightness;
                dst.pixel(x, y, c) = clampToByte(adjusted);
            }
            if (hasAlpha) {
                dst.pixel(x, y, channels - 1) = src.pixel(x, y, channels - 1);
            }
        }
    }
    return dst;
}

} // namespace imgpipe::ops
