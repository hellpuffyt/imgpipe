#include "imgpipe/image.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace imgpipe {

namespace {

bool endsWithCaseInsensitive(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
}

} // namespace

Image loadImage(const std::string& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::uint8_t* raw = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (raw == nullptr) {
        throw std::runtime_error("loadImage: failed to load '" + path + "': " +
                                  stbi_failure_reason());
    }
    std::vector<std::uint8_t> data(raw, raw + static_cast<std::size_t>(width) *
                                             static_cast<std::size_t>(height) *
                                             static_cast<std::size_t>(channels));
    stbi_image_free(raw);
    return Image(width, height, channels, std::move(data));
}

void saveImage(const std::string& path, const Image& image) {
    int ok = 0;
    if (endsWithCaseInsensitive(path, ".png")) {
        ok = stbi_write_png(path.c_str(), image.width(), image.height(), image.channels(),
                             image.data().data(), static_cast<int>(image.strideBytes()));
    } else if (endsWithCaseInsensitive(path, ".bmp")) {
        ok = stbi_write_bmp(path.c_str(), image.width(), image.height(), image.channels(),
                             image.data().data());
    } else if (endsWithCaseInsensitive(path, ".tga")) {
        ok = stbi_write_tga(path.c_str(), image.width(), image.height(), image.channels(),
                             image.data().data());
    } else if (endsWithCaseInsensitive(path, ".jpg") || endsWithCaseInsensitive(path, ".jpeg")) {
        ok = stbi_write_jpg(path.c_str(), image.width(), image.height(), image.channels(),
                             image.data().data(), 90);
    } else {
        throw std::runtime_error("saveImage: unsupported extension for '" + path +
                                  "' (use .png, .bmp, .tga, .jpg)");
    }
    if (ok == 0) {
        throw std::runtime_error("saveImage: failed to write '" + path + "'");
    }
}

} // namespace imgpipe
