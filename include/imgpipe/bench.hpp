#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "imgpipe/image.hpp"

namespace imgpipe::bench {

struct StageResult {
    std::string stageRaw;
    // Average wall-clock time of one application of this stage, in
    // seconds. When a single application completes too fast for the
    // clock to resolve reliably, this is the mean over several repeated
    // applications (see runBenchmark()) rather than a single sample.
    double wallSeconds = 0.0;
    // Megapixels of input processed per second, or a non-finite value
    // (NaN) if throughput could not be measured -- e.g. even repeated
    // application stayed at zero measured time on this clock. Always
    // check std::isfinite() before treating this as a real number;
    // compareToBaseline() already does. JSON has no infinity/NaN literal,
    // so toJson()/parseJson() round-trip a non-finite value as JSON
    // `null`, not as a fabricated number.
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
//
// A stage that completes in a few microseconds or less (small image, fast
// machine, coarse clock) is re-applied repeatedly until the *cumulative*
// elapsed time clears a measurable floor, and the reported time/throughput
// are the per-application average over that batch -- a single sub-tick
// sample would be dominated by clock quantization noise, not real work. If
// even that fails to clear the floor (a pathologically fast, trivial
// stage), StageResult::megapixelsPerSecond is set to NaN rather than to an
// invented number.
BenchReport runBenchmark(const Image& src, const std::string& spec);

// Serializes a report to a small JSON document (no external JSON library).
// A non-finite megapixelsPerSecond is written as JSON `null` (JSON has no
// infinity/NaN literal); parseJson() reads `null` back as NaN.
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
