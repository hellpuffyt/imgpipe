#include <stdexcept>

#include "imgpipe/ops.hpp"

namespace imgpipe::ops {

Image crop(const Image& src, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("crop: width/height must be positive");
    }
    if (x < 0 || y < 0 || x + w > src.width() || y + h > src.height()) {
        throw std::invalid_argument("crop: rectangle out of bounds");
    }
    const int channels = src.channels();
    Image dst(w, h, channels);
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            for (int c = 0; c < channels; ++c) {
                dst.pixel(dx, dy, c) = src.pixel(x + dx, y + dy, c);
            }
        }
    }
    return dst;
}

Image flip(const Image& src, FlipDirection direction) {
    if (src.empty()) {
        throw std::invalid_argument("flip: source image is empty");
    }
    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();
    Image dst(w, h, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int sx = direction == FlipDirection::Horizontal ? (w - 1 - x) : x;
            const int sy = direction == FlipDirection::Vertical ? (h - 1 - y) : y;
            for (int c = 0; c < channels; ++c) {
                dst.pixel(x, y, c) = src.pixel(sx, sy, c);
            }
        }
    }
    return dst;
}

Image rotate90(const Image& src, int degrees) {
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    if (normalized != 0 && normalized != 90 && normalized != 180 && normalized != 270) {
        throw std::invalid_argument("rotate90: degrees must be a multiple of 90");
    }
    if (src.empty()) {
        throw std::invalid_argument("rotate90: source image is empty");
    }

    const int w = src.width();
    const int h = src.height();
    const int channels = src.channels();

    if (normalized == 0) {
        return src;
    }
    if (normalized == 180) {
        Image dst(w, h, channels);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                for (int c = 0; c < channels; ++c) {
                    dst.pixel(x, y, c) = src.pixel(w - 1 - x, h - 1 - y, c);
                }
            }
        }
        return dst;
    }

    // 90 (clockwise) and 270 (counter-clockwise) swap dimensions.
    Image dst(h, w, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int dx = 0;
            int dy = 0;
            if (normalized == 90) {
                // (x, y) in src -> (h-1-y, x) in dst.
                dx = h - 1 - y;
                dy = x;
            } else { // 270
                // (x, y) in src -> (y, w-1-x) in dst.
                dx = y;
                dy = w - 1 - x;
            }
            for (int c = 0; c < channels; ++c) {
                dst.pixel(dx, dy, c) = src.pixel(x, y, c);
            }
        }
    }
    return dst;
}

} // namespace imgpipe::ops
