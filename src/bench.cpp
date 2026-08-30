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

} // namespace

BenchReport runBenchmark(const Image& src, const std::string& spec) {
    BenchReport report;
    Image current = src;
    const auto overallStart = std::chrono::steady_clock::now();

    for (const Stage& stage : parseOps(spec)) {
        const long inW = current.width();
        const long inH = current.height();
        const auto start = std::chrono::steady_clock::now();
        current = applyStage(current, stage);
        const auto end = std::chrono::steady_clock::now();

        const double seconds = std::chrono::duration<double>(end - start).count();
        StageResult result;
        result.stageRaw = stage.raw;
        result.wallSeconds = seconds;
        result.inputWidth = inW;
        result.inputHeight = inH;
        result.megapixelsPerSecond = seconds > 0.0 ? megapixels(inW, inH) / seconds
                                                     : std::numeric_limits<double>::infinity();
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
        out << "    {\"stageRaw\": \"" << s.stageRaw << "\", \"wallSeconds\": " << s.wallSeconds
            << ", \"megapixelsPerSecond\": " << s.megapixelsPerSecond
            << ", \"inputWidth\": " << s.inputWidth << ", \"inputHeight\": " << s.inputHeight
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
        if (b.megapixelsPerSecond <= 0.0 || !std::isfinite(b.megapixelsPerSecond)) {
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
        std::size_t end = pos;
        while (end < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[end])) || text_[end] == '-' ||
                text_[end] == '+' || text_[end] == '.' || text_[end] == 'e' || text_[end] == 'E' ||
                text_.compare(end, 3, "inf") == 0)) {
            ++end;
        }
        const std::string token = text_.substr(pos, end - pos);
        if (token == "inf" || token == "+inf") {
            return std::numeric_limits<double>::infinity();
        }
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
