#include "imgpipe/pipeline.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(parseOps_splits_stages_and_params) {
    std::vector<Stage> stages = parseOps("resize=800x600,gaussian=2.0,gray");
    CHECK(stages.size() == 3);
    CHECK(stages[0].name == "resize");
    CHECK(stages[0].params == "800x600");
    CHECK(stages[1].name == "gaussian");
    CHECK(stages[1].params == "2.0");
    CHECK(stages[2].name == "gray");
    CHECK(stages[2].params.empty());
}

IMGPIPE_TEST(parseOps_empty_spec_yields_no_stages) {
    CHECK(parseOps("").empty());
    CHECK(parseOps("   ").empty());
}

IMGPIPE_TEST(parseOps_rejects_empty_stage) {
    CHECK_THROWS(parseOps("resize=800x600,,gray"));
}

IMGPIPE_TEST(runPipeline_chains_operations_in_order) {
    Image src = makeGradient(20, 20);
    Image result = runPipeline(src, "resize=10x10:box,gray,brightness=10");
    CHECK(result.width() == 10);
    CHECK(result.height() == 10);
    CHECK(result.channels() == 1);
}

IMGPIPE_TEST(runPipeline_rejects_unknown_operation) {
    Image src = makeSolid(4, 4, 3, 1);
    CHECK_THROWS(runPipeline(src, "frobnicate=1"));
}

IMGPIPE_TEST(runPipeline_resize_rejects_malformed_dimensions) {
    Image src = makeSolid(4, 4, 3, 1);
    CHECK_THROWS(runPipeline(src, "resize=800"));
    CHECK_THROWS(runPipeline(src, "resize=abcxdef"));
    CHECK_THROWS(runPipeline(src, "resize=0x10"));
}

IMGPIPE_TEST(runPipeline_gaussian_requires_value) {
    Image src = makeSolid(4, 4, 3, 1);
    CHECK_THROWS(runPipeline(src, "gaussian"));
    CHECK_THROWS(runPipeline(src, "gaussian=notanumber"));
}

IMGPIPE_TEST(runPipeline_resize_auto_selects_box_when_downscaling) {
    Image src = makeCheckerboard(4, 4);
    Image viaAuto = runPipeline(src, "resize=2x2");
    Image viaExplicitBox = runPipeline(src, "resize=2x2:box");
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            CHECK(viaAuto.pixel(x, y, 0) == viaExplicitBox.pixel(x, y, 0));
        }
    }
}

IMGPIPE_TEST(runPipeline_convolve_named_kernels) {
    Image src = makeSolid(5, 5, 1, 128);
    Image result = runPipeline(src, "convolve=edge");
    CHECK(result.pixel(2, 2, 0) == 0);
}

IMGPIPE_TEST(runPipeline_convolve_custom_kernel) {
    Image src = makeGradient(8, 8);
    Image result = runPipeline(src, "convolve=0:0:0:0:1:0:0:0:0");
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            CHECK(result.pixel(x, y, 0) == src.pixel(x, y, 0));
        }
    }
}

IMGPIPE_TEST(runPipeline_crop_flip_rotate_stages) {
    Image src = makeGradient(10, 10);
    Image result = runPipeline(src, "crop=1:1:6:6,flip=h,rotate=90");
    CHECK(result.width() == 6);
    CHECK(result.height() == 6);
}

IMGPIPE_TEST(runPipeline_one_by_one_image_survives_full_pipeline) {
    Image src = makeSolid(1, 1, 3, 10, 20, 30);
    Image result = runPipeline(
        src, "resize=4x4:bilinear,gaussian=1.0,brightness=5,contrast=1.1,convolve=sharpen,"
             "flip=h,rotate=180,crop=1:1:2:2");
    CHECK(result.width() == 2);
    CHECK(result.height() == 2);
}
