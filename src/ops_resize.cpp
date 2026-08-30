#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "imgpipe/ops.hpp"

namespace imgpipe::ops {

namespace {

std::uint8_t clampToByte(double v) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 255.0)));
}

Image resizeBilinear(const Image& src, int newWidth, int newHeight) {
    Image dst(newWidth, newHeight, src.channels());
    const int sw = src.width();
    const int sh = src.height();
    const int channels = src.channels();

    // Scale factors mapping destination pixel centers back into source space.
    const double scaleX = static_cast<double>(sw) / static_cast<double>(newWidth);
    const double scaleY = static_cast<double>(sh) / static_cast<double>(newHeight);

    for (int dy = 0; dy < newHeight; ++dy) {
        // Map destination pixel center to source space.
        double sy = (static_cast<double>(dy) + 0.5) * scaleY - 0.5;
        sy = std::clamp(sy, 0.0, static_cast<double>(sh - 1));
        const int y0 = static_cast<int>(std::floor(sy));
        const int y1 = std::min(y0 + 1, sh - 1);
        const double fy = sy - static_cast<double>(y0);

        for (int dx = 0; dx < newWidth; ++dx) {
            double sx = (static_cast<double>(dx) + 0.5) * scaleX - 0.5;
            sx = std::clamp(sx, 0.0, static_cast<double>(sw - 1));
            const int x0 = static_cast<int>(std::floor(sx));
            const int x1 = std::min(x0 + 1, sw - 1);
            const double fx = sx - static_cast<double>(x0);

            for (int c = 0; c < channels; ++c) {
                const double v00 = src.pixel(x0, y0, c);
                const double v10 = src.pixel(x1, y0, c);
                const double v01 = src.pixel(x0, y1, c);
                const double v11 = src.pixel(x1, y1, c);
                const double top = v00 + (v10 - v00) * fx;
                const double bottom = v01 + (v11 - v01) * fx;
                const double value = top + (bottom - top) * fy;
                dst.pixel(dx, dy, c) = clampToByte(value);
            }
        }
    }
    return dst;
}

Image resizeBox(const Image& src, int newWidth, int newHeight) {
    Image dst(newWidth, newHeight, src.channels());
    const int sw = src.width();
    const int sh = src.height();
    const int channels = src.channels();

    const double scaleX = static_cast<double>(sw) / static_cast<double>(newWidth);
    const double scaleY = static_cast<double>(sh) / static_cast<double>(newHeight);

    for (int dy = 0; dy < newHeight; ++dy) {
        // Source-space extent [srcY0, srcY1) contributing to this destination row.
        double srcY0 = static_cast<double>(dy) * scaleY;
        double srcY1 = static_cast<double>(dy + 1) * scaleY;
        srcY1 = std::max(srcY1, srcY0 + 1e-9);
        const int iy0 = std::clamp(static_cast<int>(std::floor(srcY0)), 0, sh - 1);
        const int iy1 = std::clamp(static_cast<int>(std::ceil(srcY1)), iy0 + 1, sh);

        for (int dx = 0; dx < newWidth; ++dx) {
            double srcX0 = static_cast<double>(dx) * scaleX;
            double srcX1 = static_cast<double>(dx + 1) * scaleX;
            srcX1 = std::max(srcX1, srcX0 + 1e-9);
            const int ix0 = std::clamp(static_cast<int>(std::floor(srcX0)), 0, sw - 1);
            const int ix1 = std::clamp(static_cast<int>(std::ceil(srcX1)), ix0 + 1, sw);

            for (int c = 0; c < channels; ++c) {
                double sum = 0.0;
                double weightSum = 0.0;
                for (int sy = iy0; sy < iy1; ++sy) {
                    // Weight of this source row's overlap with [srcY0, srcY1).
                    const double wy = std::min(static_cast<double>(sy + 1), srcY1) -
                                       std::max(static_cast<double>(sy), srcY0);
                    if (wy <= 0.0) {
                        continue;
                    }
                    for (int sx = ix0; sx < ix1; ++sx) {
                        const double wx = std::min(static_cast<double>(sx + 1), srcX1) -
                                           std::max(static_cast<double>(sx), srcX0);
                        if (wx <= 0.0) {
                            continue;
                        }
                        const double w = wx * wy;
                        sum += w * static_cast<double>(src.pixel(sx, sy, c));
                        weightSum += w;
                    }
                }
                const double value = weightSum > 0.0 ? sum / weightSum : 0.0;
                dst.pixel(dx, dy, c) = clampToByte(value);
            }
        }
    }
    return dst;
}

} // namespace

Image resize(const Image& src, int newWidth, int newHeight, ResizeMethod method) {
    if (newWidth <= 0 || newHeight <= 0) {
        throw std::invalid_argument("resize: target dimensions must be positive");
    }
    if (src.empty()) {
        throw std::invalid_argument("resize: source image is empty");
    }
    if (src.width() == newWidth && src.height() == newHeight) {
        return src;
    }
    switch (method) {
        case ResizeMethod::Bilinear:
            return resizeBilinear(src, newWidth, newHeight);
        case ResizeMethod::Box:
            return resizeBox(src, newWidth, newHeight);
    }
    throw std::invalid_argument("resize: unknown resize method");
}

} // namespace imgpipe::ops
