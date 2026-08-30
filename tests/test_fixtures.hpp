#pragma once

#include <cstdint>

#include "imgpipe/image.hpp"

namespace imgpipe::test {

// A horizontal grayscale gradient: pixel (x, y) = round(x * 255 / (w - 1)).
// For w == 1, the single pixel is 0.
inline Image makeGradient(int w, int h) {
    Image img(w, h, 1);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t v = (w > 1) ? static_cast<std::uint8_t>((x * 255) / (w - 1))
                                             : static_cast<std::uint8_t>(0);
            img.pixel(x, y, 0) = v;
        }
    }
    return img;
}

// A 1-pixel-cell checkerboard: pixel (x, y) = 255 if (x + y) is even, else 0.
inline Image makeCheckerboard(int w, int h, int channels = 1) {
    Image img(w, h, channels);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t v = ((x + y) % 2 == 0) ? 255 : 0;
            for (int c = 0; c < channels; ++c) {
                img.pixel(x, y, c) = v;
            }
        }
    }
    return img;
}

inline Image makeSolid(int w, int h, int channels, std::uint8_t r, std::uint8_t g = 0,
                        std::uint8_t b = 0, std::uint8_t a = 255) {
    Image img(w, h, channels);
    const std::uint8_t values[4] = {r, g, b, a};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < channels; ++c) {
                img.pixel(x, y, c) = values[c];
            }
        }
    }
    return img;
}

} // namespace imgpipe::test
