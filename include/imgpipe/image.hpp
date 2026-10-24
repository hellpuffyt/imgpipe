#pragma once

#include <climits>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace imgpipe {

// A dense, row-major image buffer. Pixels are stored interleaved:
// data[(y * width + x) * channels + c].
// Supported channel counts: 1 (gray), 2 (gray+alpha), 3 (RGB), 4 (RGBA).
// Largest pixel buffer an Image may hold, in bytes. Pixel offsets are computed in int
// arithmetic throughout the operations, so the buffer must stay indexable by int.
inline constexpr std::size_t kMaxImageBytes = static_cast<std::size_t>(INT_MAX);

class Image {
public:
    Image() = default;

    Image(int width, int height, int channels)
        : width_(width), height_(height), channels_(channels),
          data_(checkedSize(width, height, channels), 0) {
        if (width < 0 || height < 0) {
            throw std::invalid_argument("Image: width/height must be non-negative");
        }
        if (channels < 1 || channels > 4) {
            throw std::invalid_argument("Image: channels must be in [1,4]");
        }
    }

    Image(int width, int height, int channels, std::vector<std::uint8_t> data)
        : width_(width), height_(height), channels_(channels), data_(std::move(data)) {
        if (width < 0 || height < 0) {
            throw std::invalid_argument("Image: width/height must be non-negative");
        }
        if (channels < 1 || channels > 4) {
            throw std::invalid_argument("Image: channels must be in [1,4]");
        }
        if (data_.size() != checkedSize(width, height, channels)) {
            throw std::invalid_argument("Image: data size does not match width*height*channels");
        }
    }

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] int channels() const noexcept { return channels_; }
    [[nodiscard]] bool empty() const noexcept { return width_ == 0 || height_ == 0; }

    [[nodiscard]] std::vector<std::uint8_t>& data() noexcept { return data_; }
    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept { return data_; }

    [[nodiscard]] std::size_t strideBytes() const noexcept {
        return static_cast<std::size_t>(width_) * static_cast<std::size_t>(channels_);
    }

    // Bounds-checked pixel component accessor.
    [[nodiscard]] std::uint8_t& at(int x, int y, int c) {
        return data_.at(index(x, y, c));
    }
    [[nodiscard]] std::uint8_t at(int x, int y, int c) const {
        return data_.at(index(x, y, c));
    }

    // Fast, unchecked pixel component accessor for hot loops. Caller must
    // guarantee (x, y, c) are in range.
    [[nodiscard]] std::uint8_t& pixel(int x, int y, int c) noexcept {
        return data_[offset(x, y, c)];
    }
    [[nodiscard]] std::uint8_t pixel(int x, int y, int c) const noexcept {
        return data_[offset(x, y, c)];
    }

    [[nodiscard]] bool hasAlpha() const noexcept { return channels_ == 2 || channels_ == 4; }

private:
    // Validates the dimensions *before* anything is allocated (it runs in the member
    // initializer list) and returns the buffer size in bytes.
    [[nodiscard]] static std::size_t checkedSize(int width, int height, int channels) {
        if (width < 0 || height < 0) {
            throw std::invalid_argument("Image: width/height must be non-negative");
        }
        if (channels < 1 || channels > 4) {
            throw std::invalid_argument("Image: channels must be in [1,4]");
        }
        const std::size_t pixels =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (pixels != 0 && pixels > kMaxImageBytes / static_cast<std::size_t>(channels)) {
            throw std::length_error("Image: " + std::to_string(width) + "x" +
                                    std::to_string(height) + "x" + std::to_string(channels) +
                                    " exceeds the 2 GiB image size limit");
        }
        return pixels * static_cast<std::size_t>(channels);
    }

    [[nodiscard]] std::size_t offset(int x, int y, int c) const noexcept {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(x)) *
                   static_cast<std::size_t>(channels_) +
               static_cast<std::size_t>(c);
    }

    [[nodiscard]] std::size_t index(int x, int y, int c) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || c < 0 || c >= channels_) {
            throw std::out_of_range("Image: pixel index out of range");
        }
        return offset(x, y, c);
    }

    int width_ = 0;
    int height_ = 0;
    int channels_ = 0;
    std::vector<std::uint8_t> data_;
};

// Loads an image from disk (PNG/JPEG/BMP/TGA via stb_image). Throws
// std::runtime_error on failure.
// Largest image loadImage() will decode, in pixels (256 Mpx). The header is checked before
// any pixel data is decoded, so a small file that declares huge dimensions is rejected
// instead of allocating gigabytes.
inline constexpr long long kMaxDecodedPixels = 256LL * 1024 * 1024;

// Decodes PNG, JPEG, BMP or TGA. Throws std::runtime_error on unreadable, truncated,
// malformed, unsupported or oversized input.
Image loadImage(const std::string& path);

// Saves an image to disk. Format is inferred from the file extension
// (.png, .bmp, .tga, .jpg/.jpeg). Throws std::runtime_error on failure.
void saveImage(const std::string& path, const Image& image);

} // namespace imgpipe
