#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace imgpipe {

// A dense, row-major image buffer. Pixels are stored interleaved:
// data[(y * width + x) * channels + c].
// Supported channel counts: 1 (gray), 2 (gray+alpha), 3 (RGB), 4 (RGBA).
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
        return data_[static_cast<std::size_t>((y * width_ + x) * channels_ + c)];
    }
    [[nodiscard]] std::uint8_t pixel(int x, int y, int c) const noexcept {
        return data_[static_cast<std::size_t>((y * width_ + x) * channels_ + c)];
    }

    [[nodiscard]] bool hasAlpha() const noexcept { return channels_ == 2 || channels_ == 4; }

private:
    [[nodiscard]] static std::size_t checkedSize(int width, int height, int channels) {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
               static_cast<std::size_t>(channels);
    }

    [[nodiscard]] std::size_t index(int x, int y, int c) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || c < 0 || c >= channels_) {
            throw std::out_of_range("Image: pixel index out of range");
        }
        return static_cast<std::size_t>((y * width_ + x) * channels_ + c);
    }

    int width_ = 0;
    int height_ = 0;
    int channels_ = 0;
    std::vector<std::uint8_t> data_;
};

// Loads an image from disk (PNG/JPEG/BMP/TGA via stb_image). Throws
// std::runtime_error on failure.
Image loadImage(const std::string& path);

// Saves an image to disk. Format is inferred from the file extension
// (.png, .bmp, .tga, .jpg/.jpeg). Throws std::runtime_error on failure.
void saveImage(const std::string& path, const Image& image);

} // namespace imgpipe
