#include <cmath>
#include <limits>

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

// Regression test for a real bug: a stage that measures at (or is
// constructed with) zero wall-clock time used to be encoded as throughput
// = +infinity, and toJson() wrote that out as the bare word "inf" via
// operator<<. That is not valid JSON, and parseJson() -- including its
// std::from_chars-based number parser -- rejected it outright ("malformed
// number 'i'"), so a benchmark report the tool itself produced could not
// always be read back by the tool. This is exactly the round-trip that
// would have caught it before it shipped.
IMGPIPE_TEST(bench_json_roundtrip_zero_duration_stage_does_not_throw) {
    bench::BenchReport report;
    report.totalWallSeconds = 0.0;
    report.peakResidentBytes = 12345;
    bench::StageResult zeroDuration;
    zeroDuration.stageRaw = "gray";
    zeroDuration.wallSeconds = 0.0;
    zeroDuration.megapixelsPerSecond = std::numeric_limits<double>::quiet_NaN();
    zeroDuration.inputWidth = 4;
    zeroDuration.inputHeight = 4;
    report.stages.push_back(zeroDuration);

    const std::string json = bench::toJson(report);
    // The unmeasurable stage must be encoded as JSON `null`, never as the
    // literal (invalid-JSON) text "inf" or "nan".
    CHECK(json.find("\"megapixelsPerSecond\": null") != std::string::npos);
    CHECK(json.find("inf") == std::string::npos);
    CHECK(json.find("nan") == std::string::npos);

    bench::BenchReport parsed;
    CHECK_NOTHROW(parsed = bench::parseJson(json));
    CHECK(parsed.stages.size() == 1);
    CHECK(!std::isfinite(parsed.stages[0].megapixelsPerSecond));
    CHECK(parsed.stages[0].stageRaw == "gray");
}

IMGPIPE_TEST(bench_parseJson_accepts_legacy_inf_literal) {
    // toJson() no longer emits the bare "inf" token, but parseJson() still
    // accepts it so a baseline JSON file written before this fix keeps
    // working rather than becoming permanently unreadable.
    const std::string legacyJson =
        "{\n"
        "  \"totalWallSeconds\": 0.0,\n"
        "  \"peakResidentBytes\": -1,\n"
        "  \"stages\": [\n"
        "    {\"stageRaw\": \"gray\", \"wallSeconds\": 0, \"megapixelsPerSecond\": inf, "
        "\"inputWidth\": 4, \"inputHeight\": 4}\n"
        "  ]\n"
        "}\n";
    bench::BenchReport parsed;
    CHECK_NOTHROW(parsed = bench::parseJson(legacyJson));
    CHECK(!std::isfinite(parsed.stages[0].megapixelsPerSecond));
}

IMGPIPE_TEST(bench_compareToBaseline_skips_when_current_is_nonfinite) {
    // Coherence with the null/NaN encoding: an unmeasurable *current* run
    // must be skipped, not compared as if it were a real (and enormous)
    // throughput number.
    bench::BenchReport baseline;
    baseline.stages.push_back(bench::StageResult{"gray", 0.01, 100.0, 1000, 1000});
    bench::BenchReport unmeasurableCurrent;
    unmeasurableCurrent.stages.push_back(
        bench::StageResult{"gray", 0.0, std::numeric_limits<double>::quiet_NaN(), 1000, 1000});

    auto regressions = bench::compareToBaseline(baseline, unmeasurableCurrent, 15.0);
    CHECK(regressions.empty());
}
