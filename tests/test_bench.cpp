#include "imgpipe/bench.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

IMGPIPE_TEST(bench_runBenchmark_reports_one_stage_per_op) {
    Image src = makeGradient(32, 32);
    bench::BenchReport report = bench::runBenchmark(src, "resize=16x16:box,gray");
    CHECK(report.stages.size() == 2);
    CHECK(report.stages[0].stageRaw == "resize=16x16:box");
    CHECK(report.stages[1].stageRaw == "gray");
    CHECK(report.stages[0].inputWidth == 32);
    CHECK(report.stages[1].inputWidth == 16);
    CHECK(report.totalWallSeconds >= 0.0);
    for (const auto& s : report.stages) {
        CHECK(s.megapixelsPerSecond > 0.0);
    }
}

IMGPIPE_TEST(bench_json_roundtrip_preserves_values) {
    Image src = makeGradient(16, 16);
    bench::BenchReport original = bench::runBenchmark(src, "gray,brightness=10");
    const std::string json = bench::toJson(original);
    bench::BenchReport parsed = bench::parseJson(json);

    CHECK(parsed.stages.size() == original.stages.size());
    CHECK_NEAR(parsed.totalWallSeconds, original.totalWallSeconds, 1e-6);
    for (std::size_t i = 0; i < original.stages.size(); ++i) {
        CHECK(parsed.stages[i].stageRaw == original.stages[i].stageRaw);
        CHECK_NEAR(parsed.stages[i].megapixelsPerSecond, original.stages[i].megapixelsPerSecond, 1e-6);
        CHECK(parsed.stages[i].inputWidth == original.stages[i].inputWidth);
        CHECK(parsed.stages[i].inputHeight == original.stages[i].inputHeight);
    }
}

IMGPIPE_TEST(bench_compareToBaseline_detects_regression) {
    bench::BenchReport baseline;
    baseline.stages.push_back(bench::StageResult{"gray", 0.01, 100.0, 1000, 1000});
    bench::BenchReport slower;
    slower.stages.push_back(bench::StageResult{"gray", 0.02, 50.0, 1000, 1000});

    auto regressions = bench::compareToBaseline(baseline, slower, 15.0);
    CHECK(regressions.size() == 1);
    CHECK(regressions[0].stageRaw == "gray");
    CHECK_NEAR(regressions[0].percentSlower, 50.0, 0.001);
}

IMGPIPE_TEST(bench_compareToBaseline_no_regression_within_threshold) {
    bench::BenchReport baseline;
    baseline.stages.push_back(bench::StageResult{"gray", 0.01, 100.0, 1000, 1000});
    bench::BenchReport slightlySlower;
    slightlySlower.stages.push_back(bench::StageResult{"gray", 0.0105, 95.0, 1000, 1000});

    auto regressions = bench::compareToBaseline(baseline, slightlySlower, 15.0);
    CHECK(regressions.empty());
}
