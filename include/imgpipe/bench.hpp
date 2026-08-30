#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "imgpipe/image.hpp"

namespace imgpipe::bench {

struct StageResult {
    std::string stageRaw;
    double wallSeconds = 0.0;
    double megapixelsPerSecond = 0.0;
    long inputWidth = 0;
    long inputHeight = 0;
};

struct BenchReport {
    std::vector<StageResult> stages;
    double totalWallSeconds = 0.0;
    std::int64_t peakResidentBytes = -1; // -1 if unobtainable on this platform
};

// Runs each stage of `spec` against `src` in order, timing each stage
// individually with a monotonic clock. Returns per-stage throughput in
// megapixels of *input* processed per second (a fairer efficiency metric
// than output pixels, since a downscale processes many input pixels per
// output pixel).
BenchReport runBenchmark(const Image& src, const std::string& spec);

// Serializes a report to a small JSON document (no external JSON library).
std::string toJson(const BenchReport& report);

// Result of comparing a fresh report against a stored baseline.
struct Regression {
    std::string stageRaw;
    double baselineMpps = 0.0;
    double currentMpps = 0.0;
    double percentSlower = 0.0;
};

// Compares `current` against `baseline` stage by stage (matched by stageRaw
// order). A stage regresses if its megapixels/second drops by more than
// `thresholdPercent` relative to the baseline. Returns the list of
// regressing stages (empty if none).
std::vector<Regression> compareToBaseline(const BenchReport& baseline, const BenchReport& current,
                                           double thresholdPercent);

// Parses a BenchReport previously written by toJson(). Throws
// std::runtime_error on malformed input.
BenchReport parseJson(const std::string& json);

} // namespace imgpipe::bench
