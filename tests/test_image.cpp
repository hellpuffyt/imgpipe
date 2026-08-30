#include "imgpipe/image.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(image_construct_and_index) {
    Image img(3, 2, 3);
    CHECK(img.width() == 3);
    CHECK(img.height() == 2);
    CHECK(img.channels() == 3);
    CHECK(!img.empty());
    img.at(1, 1, 2) = 200;
    CHECK(img.at(1, 1, 2) == 200);
}

IMGPIPE_TEST(image_out_of_range_throws) {
    Image img(3, 2, 3);
    CHECK_THROWS(img.at(3, 0, 0));
    CHECK_THROWS(img.at(0, 2, 0));
    CHECK_THROWS(img.at(0, 0, 3));
    CHECK_THROWS(img.at(-1, 0, 0));
}

IMGPIPE_TEST(image_rejects_invalid_channels) {
    CHECK_THROWS(Image(2, 2, 0));
    CHECK_THROWS(Image(2, 2, 5));
}

IMGPIPE_TEST(image_rejects_mismatched_data_size) {
    std::vector<std::uint8_t> data(10, 0);
    CHECK_THROWS(Image(3, 3, 3, data));
}

IMGPIPE_TEST(image_one_by_one_construct) {
    Image img(1, 1, 4);
    CHECK(img.width() == 1);
    CHECK(img.height() == 1);
    CHECK(!img.empty());
}

IMGPIPE_TEST(image_zero_dimension_is_empty) {
    Image img(0, 0, 3);
    CHECK(img.empty());
}

IMGPIPE_TEST(image_save_and_load_roundtrip_png) {
    Image original = makeCheckerboard(6, 6, 3);
    const std::string path = "test_roundtrip.png";
    saveImage(path, original);
    Image loaded = loadImage(path);
    CHECK(loaded.width() == original.width());
    CHECK(loaded.height() == original.height());
    CHECK(loaded.channels() == original.channels());
    for (int y = 0; y < original.height(); ++y) {
        for (int x = 0; x < original.width(); ++x) {
            for (int c = 0; c < original.channels(); ++c) {
                CHECK(loaded.pixel(x, y, c) == original.pixel(x, y, c));
            }
        }
    }
}

IMGPIPE_TEST(image_save_rejects_unknown_extension) {
    Image img = makeSolid(2, 2, 3, 1);
    CHECK_THROWS(saveImage("out.weird", img));
}

IMGPIPE_TEST(image_load_missing_file_throws) {
    CHECK_THROWS(loadImage("does_not_exist_at_all.png"));
}
