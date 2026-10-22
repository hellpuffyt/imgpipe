// Regression tests for malformed input and overflow handling. They are most
// useful under -DIMGPIPE_ENABLE_SANITIZERS=ON, where any out-of-bounds access
// or undefined behaviour aborts the run.
#include <climits>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "imgpipe/image.hpp"
#include "imgpipe/ops.hpp"
#include "imgpipe/pipeline.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

namespace {

std::vector<std::uint8_t> readBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

// Loading must either succeed with a self-consistent image or throw
// std::runtime_error -- never crash, hang, or throw anything else.
bool loadIsSafe(const std::string& path) {
    try {
        const Image img = loadImage(path);
        return img.data().size() == static_cast<std::size_t>(img.width()) *
                                        static_cast<std::size_t>(img.height()) *
                                        static_cast<std::size_t>(img.channels());
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::uint8_t> encodedSample(const std::string& extension) {
    const std::string path = "hardening_sample" + extension;
    saveImage(path, makeCheckerboard(37, 23, 3));
    return readBytes(path);
}

void putBigEndian32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    b[at] = static_cast<std::uint8_t>(v >> 24);
    b[at + 1] = static_cast<std::uint8_t>(v >> 16);
    b[at + 2] = static_cast<std::uint8_t>(v >> 8);
    b[at + 3] = static_cast<std::uint8_t>(v);
}

void putLittleEndian32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
    b[at + 2] = static_cast<std::uint8_t>(v >> 16);
    b[at + 3] = static_cast<std::uint8_t>(v >> 24);
}

} // namespace

IMGPIPE_TEST(load_roundtrip_every_supported_format) {
    const Image original = makeCheckerboard(9, 5, 3);
    for (const std::string ext : {".png", ".bmp", ".tga", ".jpg"}) {
        const std::string path = "hardening_roundtrip" + ext;
        saveImage(path, original);
        const Image loaded = loadImage(path);
        CHECK(loaded.width() == 9);
        CHECK(loaded.height() == 5);
        if (ext != ".jpg") {  // JPEG is lossy; the others must be bit-exact.
            CHECK(loaded.data() == original.data());
        }
    }
}

IMGPIPE_TEST(load_truncated_files_never_crash) {
    for (const std::string ext : {".png", ".bmp", ".tga", ".jpg"}) {
        const std::vector<std::uint8_t> full = encodedSample(ext);
        const std::string path = "hardening_truncated" + ext;
        const std::size_t step = full.size() / 64 + 1;
        for (std::size_t len = 0; len < full.size(); len += (len < 64 ? 1 : step)) {
            writeBytes(path, std::vector<std::uint8_t>(full.begin(),
                                                       full.begin() + static_cast<long>(len)));
            CHECK(loadIsSafe(path));
        }
    }
}

IMGPIPE_TEST(load_corrupted_files_never_crash) {
    std::uint32_t state = 12345u;  // fixed seed: deterministic corpus
    auto next = [&state]() {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    };
    for (const std::string ext : {".png", ".bmp", ".tga", ".jpg"}) {
        const std::vector<std::uint8_t> full = encodedSample(ext);
        const std::string path = "hardening_corrupt" + ext;
        for (int round = 0; round < 150; ++round) {
            std::vector<std::uint8_t> bytes = full;
            const int flips = 1 + static_cast<int>(next() % 8);
            for (int i = 0; i < flips; ++i) {
                // Bias towards the header, where dimensions and table sizes live.
                const std::size_t span = (next() % 4 == 0) ? bytes.size() : std::min<std::size_t>(bytes.size(), 128);
                bytes[next() % span] = static_cast<std::uint8_t>(next());
            }
            writeBytes(path, bytes);
            CHECK(loadIsSafe(path));
        }
    }
}

IMGPIPE_TEST(load_rejects_header_declaring_huge_dimensions) {
    // A ~200 byte PNG whose header claims 20000x20000 pixels must be rejected
    // from the header alone, not decoded into a 1.2 GB buffer.
    std::vector<std::uint8_t> png = encodedSample(".png");
    putBigEndian32(png, 16, 20000);
    putBigEndian32(png, 20, 20000);
    writeBytes("hardening_bomb.png", png);
    CHECK_THROWS(loadImage("hardening_bomb.png"));

    // Regression: a 2.6 KB BMP that declared 37x12255255 pixels used to be
    // "decoded" (stb pads missing rows) into ~2 GB of memory.
    std::vector<std::uint8_t> bmp = encodedSample(".bmp");
    putLittleEndian32(bmp, 22, 12255255);
    writeBytes("hardening_bomb.bmp", bmp);
    CHECK_THROWS(loadImage("hardening_bomb.bmp"));
}

IMGPIPE_TEST(load_rejects_formats_that_are_not_documented) {
    std::vector<std::uint8_t> gif = {'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0, 0, 0, 0};
    writeBytes("hardening.gif", gif);
    CHECK_THROWS(loadImage("hardening.gif"));
    writeBytes("hardening_empty.png", {});
    CHECK_THROWS(loadImage("hardening_empty.png"));
}

IMGPIPE_TEST(image_rejects_oversized_or_negative_dimensions_before_allocating) {
    CHECK_THROWS(Image(100000, 100000, 3));
    CHECK_THROWS(Image(INT_MAX, INT_MAX, 4));
    CHECK_THROWS(Image(-1, 5, 3));
    CHECK_THROWS(Image(5, -1, 3));
    CHECK_THROWS(ops::resize(makeSolid(2, 2, 3, 0), 100000, 100000, ops::ResizeMethod::Bilinear));
}

// Regression: x + w overflowed int, slipped past the bounds check and read
// out of bounds (SIGBUS in a release build).
IMGPIPE_TEST(crop_rejects_rectangles_that_overflow_int) {
    const Image src = makeSolid(8, 8, 3, 10);
    CHECK_THROWS(ops::crop(src, INT_MAX, 0, 1, 1));
    CHECK_THROWS(ops::crop(src, 1, 1, INT_MAX, INT_MAX));
    CHECK_THROWS(ops::crop(src, 0, INT_MAX, 1, 1));
    CHECK_THROWS(runPipeline(src, "crop=2147483647:0:1:1"));
    const Image whole = ops::crop(src, 0, 0, 8, 8);
    CHECK(whole.data() == src.data());
}

// Regression: a non-finite or huge sigma was converted to int (undefined
// behaviour) when sizing the kernel.
IMGPIPE_TEST(gaussian_rejects_non_finite_and_huge_sigma) {
    const Image src = makeSolid(4, 4, 1, 50);
    CHECK_THROWS(ops::gaussianBlur(src, std::numeric_limits<double>::quiet_NaN()));
    CHECK_THROWS(ops::gaussianBlur(src, std::numeric_limits<double>::infinity()));
    CHECK_THROWS(ops::gaussianBlur(src, 1e30));
    CHECK_THROWS(ops::gaussianBlur(src, ops::kMaxGaussianSigma * 2));
    CHECK_NOTHROW(ops::gaussianBlur(src, 2.0));
}

IMGPIPE_TEST(pipeline_rejects_non_finite_numbers) {
    const Image src = makeSolid(4, 4, 3, 50);
    CHECK_THROWS(runPipeline(src, "gaussian=nan"));
    CHECK_THROWS(runPipeline(src, "gaussian=inf"));
    CHECK_THROWS(runPipeline(src, "brightness=nan"));
    CHECK_THROWS(runPipeline(src, "contrast=-inf"));
    CHECK_THROWS(runPipeline(src, "convolve=nan:0:0:0:1:0:0:0:0"));
    CHECK_THROWS(runPipeline(src, "brightness=1e999"));
    CHECK_NOTHROW(runPipeline(src, "brightness=-20,contrast=1.5"));
}
