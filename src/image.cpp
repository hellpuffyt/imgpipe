#include "imgpipe/image.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstring>
#include <fstream>
#include <iterator>

// Only the formats imgpipe documents are compiled in; this also keeps stb_image's
// GIF/PSD/HDR/PIC/PNM decoders (and their attack surface) out of the binary.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
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
    // Read the file once and decode from memory, so the header that is checked against the
    // size limit is the header that gets decoded.
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("loadImage: failed to open '" + path + "'");
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    if (bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::runtime_error("loadImage: '" + path + "' is larger than 2 GiB");
    }
    const auto* buffer = reinterpret_cast<const stbi_uc*>(bytes.data());
    const int length = static_cast<int>(bytes.size());

    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(buffer, length, &width, &height, &channels) == 0) {
        throw std::runtime_error("loadImage: failed to load '" + path + "': " +
                                  stbi_failure_reason());
    }
    if (static_cast<long long>(width) * static_cast<long long>(height) > kMaxDecodedPixels) {
        throw std::runtime_error("loadImage: '" + path + "' declares " + std::to_string(width) +
                                  "x" + std::to_string(height) +
                                  " pixels, above the decode limit of " +
                                  std::to_string(kMaxDecodedPixels) + " pixels");
    }
    std::uint8_t* raw = stbi_load_from_memory(buffer, length, &width, &height, &channels, 0);
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
