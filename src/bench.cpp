#include "imgpipe/bench.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "imgpipe/pipeline.hpp"

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace imgpipe::bench {

namespace {

std::int64_t currentPeakRssBytes() {
#if defined(__APPLE__)
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
    return static_cast<std::int64_t>(usage.ru_maxrss); // macOS reports bytes.
#elif defined(__unix__)
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
    return static_cast<std::int64_t>(usage.ru_maxrss) * 1024; // Linux reports KB.
#else
    return -1;
#endif
}

double megapixels(long width, long height) {
    return static_cast<double>(width) * static_cast<double>(height) / 1'000'000.0;
}

// A single application of a cheap stage on a small image can complete in
// far less than one clock tick, especially on a coarse-resolution
// steady_clock. Measuring that as "0 seconds" and then dividing by it
// produces a meaningless "infinite" throughput. Instead, re-apply the
// stage until the *cumulative* time clears this floor, then average --
// several real, summed measurements are trustworthy even if any single one
// individually is not.
constexpr double kMinMeasurableSeconds = 0.002; // 2 ms
constexpr int kMaxMeasurementIterations = 200'000;

} // namespace

BenchReport runBenchmark(const Image& src, const std::string& spec) {
    BenchReport report;
    Image current = src;
    const auto overallStart = std::chrono::steady_clock::now();

    for (const Stage& stage : parseOps(spec)) {
        const long inW = current.width();
        const long inH = current.height();

        Image output;
        int iterations = 0;
        double elapsedSeconds = 0.0;
        const auto batchStart = std::chrono::steady_clock::now();
        do {
            output = applyStage(current, stage);
            ++iterations;
            elapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - batchStart)
                    .count();
        } while (elapsedSeconds < kMinMeasurableSeconds && iterations < kMaxMeasurementIterations);
        current = std::move(output);

        StageResult result;
        result.stageRaw = stage.raw;
        result.inputWidth = inW;
        result.inputHeight = inH;
        if (elapsedSeconds > 0.0) {
            result.wallSeconds = elapsedSeconds / static_cast<double>(iterations);
            result.megapixelsPerSecond = megapixels(inW, inH) / result.wallSeconds;
        } else {
            // Even kMaxMeasurementIterations repetitions stayed at zero
            // measured time: genuinely unmeasurable on this clock, not a
            // stage that is "infinitely fast". Say so explicitly rather
            // than reporting a number.
            result.wallSeconds = 0.0;
            result.megapixelsPerSecond = std::numeric_limits<double>::quiet_NaN();
        }
        report.stages.push_back(result);
    }

    const auto overallEnd = std::chrono::steady_clock::now();
    report.totalWallSeconds = std::chrono::duration<double>(overallEnd - overallStart).count();
    report.peakResidentBytes = currentPeakRssBytes();
    return report;
}

std::string toJson(const BenchReport& report) {
    std::ostringstream out;
    out.precision(std::numeric_limits<double>::max_digits10);
    out << "{\n";
    out << "  \"totalWallSeconds\": " << report.totalWallSeconds << ",\n";
    out << "  \"peakResidentBytes\": " << report.peakResidentBytes << ",\n";
    out << "  \"stages\": [\n";
    for (std::size_t i = 0; i < report.stages.size(); ++i) {
        const StageResult& s = report.stages[i];
        // JSON has no infinity/NaN literal. operator<< would emit the
        // literal text "inf"/"nan" for a non-finite double, which is not
        // valid JSON and which nothing (including our own parser) can
        // read back as a number -- so a non-finite (unmeasurable)
        // throughput is written as `null` instead, the honest "no value"
        // encoding.
        out << "    {\"stageRaw\": \"" << s.stageRaw << "\", \"wallSeconds\": " << s.wallSeconds
            << ", \"megapixelsPerSecond\": ";
        if (std::isfinite(s.megapixelsPerSecond)) {
            out << s.megapixelsPerSecond;
        } else {
            out << "null";
        }
        out << ", \"inputWidth\": " << s.inputWidth << ", \"inputHeight\": " << s.inputHeight
            << "}";
        if (i + 1 != report.stages.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

std::vector<Regression> compareToBaseline(const BenchReport& baseline, const BenchReport& current,
                                           double thresholdPercent) {
    std::vector<Regression> regressions;
    const std::size_t n = std::min(baseline.stages.size(), current.stages.size());
    for (std::size_t i = 0; i < n; ++i) {
        const StageResult& b = baseline.stages[i];
        const StageResult& c = current.stages[i];
        // A non-finite (unmeasurable) throughput on either side carries no
        // usable signal: skip the stage rather than comparing against, or
        // producing, a meaningless number.
        if (b.megapixelsPerSecond <= 0.0 || !std::isfinite(b.megapixelsPerSecond) ||
            !std::isfinite(c.megapixelsPerSecond)) {
            continue;
        }
        const double percentSlower =
            (b.megapixelsPerSecond - c.megapixelsPerSecond) / b.megapixelsPerSecond * 100.0;
        if (percentSlower > thresholdPercent) {
            regressions.push_back(Regression{c.stageRaw, b.megapixelsPerSecond,
                                              c.megapixelsPerSecond, percentSlower});
        }
    }
    return regressions;
}

namespace {

// Extremely small, tolerant JSON reader for the specific schema toJson()
// produces. Not a general-purpose JSON parser.
class SimpleJsonReader {
public:
    explicit SimpleJsonReader(const std::string& text) : text_(text) {}

    double readNumberField(const std::string& key, std::size_t from = 0) {
        const std::size_t keyPos = findKey(key, from);
        std::size_t pos = keyPos;
        skipToValueStart(pos);
        return readNumber(pos);
    }

    std::string readStringField(const std::string& key, std::size_t from = 0) {
        const std::size_t keyPos = findKey(key, from);
        std::size_t pos = keyPos;
        skipToValueStart(pos);
        return readString(pos);
    }

    std::size_t findKey(const std::string& key, std::size_t from) const {
        const std::string needle = "\"" + key + "\"";
        const std::size_t pos = text_.find(needle, from);
        if (pos == std::string::npos) {
            throw std::runtime_error("parseJson: missing key '" + key + "'");
        }
        return pos + needle.size();
    }

    std::size_t findArray(const std::string& key) const {
        const std::size_t keyPos = findKey(key, 0);
        const std::size_t bracket = text_.find('[', keyPos);
        if (bracket == std::string::npos) {
            throw std::runtime_error("parseJson: array '" + key + "' not found");
        }
        return bracket;
    }

    std::vector<std::size_t> objectStartsInArray(std::size_t arrayStart) const {
        std::vector<std::size_t> starts;
        std::size_t depth = 0;
        std::size_t i = arrayStart;
        std::size_t end = text_.find(']', arrayStart);
        if (end == std::string::npos) {
            throw std::runtime_error("parseJson: unterminated array");
        }
        while (i < end) {
            if (text_[i] == '{') {
                if (depth == 0) {
                    starts.push_back(i);
                }
                ++depth;
            } else if (text_[i] == '}') {
                if (depth > 0) {
                    --depth;
                }
            }
            ++i;
        }
        return starts;
    }

private:
    void skipToValueStart(std::size_t& pos) const {
        pos = text_.find(':', pos);
        if (pos == std::string::npos) {
            throw std::runtime_error("parseJson: malformed field");
        }
        ++pos;
        while (pos < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos]))) {
            ++pos;
        }
    }

    double readNumber(std::size_t pos) const {
        // `null` is how toJson() encodes a non-finite (unmeasurable)
        // value -- JSON has no infinity/NaN literal. Recognize whole-token
        // literals (null, and the legacy inf/+inf/-inf spellings some
        // older baseline files may still contain) by direct comparison
        // *before* falling into the digit-class scan below: that scan's
        // loop condition can only ever match a single "inf" at its
        // starting position (text_.compare(end, 3, "inf") is only true
        // while end == pos), so it never actually advances past the
        // first letter of a bare "inf"/"null" token on its own.
        for (const char* literal : {"null"}) {
            const std::size_t len = std::char_traits<char>::length(literal);
            if (text_.compare(pos, len, literal) == 0) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
        for (const char* literal : {"+inf", "-inf", "inf"}) {
            const std::size_t len = std::char_traits<char>::length(literal);
            if (text_.compare(pos, len, literal) == 0) {
                // Accepted for backward compatibility with baseline files
                // written before non-finite throughput was encoded as
                // `null`; toJson() itself never emits this anymore.
                return literal[0] == '-' ? -std::numeric_limits<double>::infinity()
                                          : std::numeric_limits<double>::infinity();
            }
        }

        std::size_t end = pos;
        while (end < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[end])) || text_[end] == '-' ||
                text_[end] == '+' || text_[end] == '.' || text_[end] == 'e' || text_[end] == 'E')) {
            ++end;
        }
        const std::string token = text_.substr(pos, end - pos);
        // std::from_chars is locale-independent (always '.' as the decimal
        // point), unlike std::stod, which defers to the C library's
        // strtod and therefore the process's current C locale.
        double value = 0.0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc()) {
            throw std::runtime_error("parseJson: malformed number '" + token + "'");
        }
        return value;
    }

    std::string readString(std::size_t pos) const {
        if (pos >= text_.size() || text_[pos] != '"') {
            throw std::runtime_error("parseJson: expected string");
        }
        const std::size_t end = text_.find('"', pos + 1);
        if (end == std::string::npos) {
            throw std::runtime_error("parseJson: unterminated string");
        }
        return text_.substr(pos + 1, end - pos - 1);
    }

    const std::string& text_;
};

} // namespace

BenchReport parseJson(const std::string& json) {
    SimpleJsonReader reader(json);
    BenchReport report;
    report.totalWallSeconds = reader.readNumberField("totalWallSeconds");
    report.peakResidentBytes = static_cast<std::int64_t>(reader.readNumberField("peakResidentBytes"));

    const std::size_t arrayStart = reader.findArray("stages");
    for (std::size_t objStart : reader.objectStartsInArray(arrayStart)) {
        StageResult s;
        s.stageRaw = reader.readStringField("stageRaw", objStart);
        s.wallSeconds = reader.readNumberField("wallSeconds", objStart);
        s.megapixelsPerSecond = reader.readNumberField("megapixelsPerSecond", objStart);
        s.inputWidth = static_cast<long>(reader.readNumberField("inputWidth", objStart));
        s.inputHeight = static_cast<long>(reader.readNumberField("inputHeight", objStart));
        report.stages.push_back(s);
    }
    return report;
}

} // namespace imgpipe::bench
