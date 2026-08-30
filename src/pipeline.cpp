#include "imgpipe/pipeline.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <stdexcept>

#include "imgpipe/ops.hpp"

namespace imgpipe {

namespace {

std::vector<std::string> splitOn(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

std::string trim(const std::string& s) {
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

// std::stod/std::stoi ultimately defer to the C library's strtod/strtol,
// whose notion of "digit" and decimal-point character follows the
// process's current C locale. That makes them unsuitable for parsing a
// command-line mini-language: a locale where the decimal separator is ','
// (e.g. de_DE) would make std::stod stop consuming at the '.' in "2.0",
// which our "fully consumed" check then (correctly, but confusingly)
// rejects -- or worse, silently accepts a different value than the user
// typed. std::from_chars is specified to be locale-independent (it always
// uses '.'  as the decimal point and never groups digits), so it is used
// here instead.
double parseDouble(const std::string& text, const std::string& stageRaw, const std::string& what) {
    double value = 0.0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        throw std::invalid_argument("invalid pipeline stage '" + stageRaw + "': expected a " +
                                     "numeric value for " + what + ", got '" + text + "'");
    }
    return value;
}

int parseInt(const std::string& text, const std::string& stageRaw, const std::string& what) {
    int value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        throw std::invalid_argument("invalid pipeline stage '" + stageRaw + "': expected an " +
                                     "integer value for " + what + ", got '" + text + "'");
    }
    return value;
}

std::pair<int, int> parseDimensions(const std::string& text, const std::string& stageRaw) {
    const auto xPos = text.find('x');
    if (xPos == std::string::npos) {
        throw std::invalid_argument("invalid pipeline stage '" + stageRaw +
                                     "': expected WIDTHxHEIGHT, got '" + text + "'");
    }
    const int w = parseInt(text.substr(0, xPos), stageRaw, "width");
    const int h = parseInt(text.substr(xPos + 1), stageRaw, "height");
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("invalid pipeline stage '" + stageRaw +
                                     "': width and height must be positive");
    }
    return {w, h};
}

} // namespace

std::vector<Stage> parseOps(const std::string& spec) {
    std::vector<Stage> stages;
    if (trim(spec).empty()) {
        return stages;
    }
    for (const std::string& rawStagePart : splitOn(spec, ',')) {
        const std::string rawStage = trim(rawStagePart);
        if (rawStage.empty()) {
            throw std::invalid_argument("invalid --ops spec: empty stage (check for stray commas)");
        }
        const auto eqPos = rawStage.find('=');
        Stage stage;
        stage.raw = rawStage;
        if (eqPos == std::string::npos) {
            stage.name = rawStage;
            stage.params = "";
        } else {
            stage.name = trim(rawStage.substr(0, eqPos));
            stage.params = trim(rawStage.substr(eqPos + 1));
        }
        if (stage.name.empty()) {
            throw std::invalid_argument("invalid --ops spec: stage '" + rawStage +
                                         "' has no operation name");
        }
        stages.push_back(std::move(stage));
    }
    return stages;
}

Image applyStage(const Image& image, const Stage& stage) {
    const std::string& name = stage.name;
    const std::string& p = stage.params;
    const std::string& raw = stage.raw;

    if (name == "resize") {
        // resize=WIDTHxHEIGHT[:bilinear|box]
        std::string dims = p;
        ops::ResizeMethod method;
        bool methodExplicit = false;
        const auto colonPos = p.find(':');
        if (colonPos != std::string::npos) {
            dims = p.substr(0, colonPos);
            const std::string methodText = p.substr(colonPos + 1);
            if (methodText == "bilinear") {
                method = ops::ResizeMethod::Bilinear;
            } else if (methodText == "box") {
                method = ops::ResizeMethod::Box;
            } else {
                throw std::invalid_argument("invalid pipeline stage '" + raw +
                                             "': resize method must be 'bilinear' or 'box', got '" +
                                             methodText + "'");
            }
            methodExplicit = true;
        }
        const auto [w, h] = parseDimensions(dims, raw);
        if (!methodExplicit) {
            // Auto-select: box for downscale (either dimension shrinks), bilinear otherwise.
            const bool downscaling = w < image.width() || h < image.height();
            method = downscaling ? ops::ResizeMethod::Box : ops::ResizeMethod::Bilinear;
        }
        return ops::resize(image, w, h, method);
    }

    if (name == "gaussian") {
        // gaussian=SIGMA
        if (p.empty()) {
            throw std::invalid_argument("invalid pipeline stage '" + raw +
                                         "': gaussian requires a sigma value, e.g. gaussian=2.0");
        }
        const double sigma = parseDouble(p, raw, "sigma");
        return ops::gaussianBlur(image, sigma);
    }

    if (name == "gray" || name == "grayscale") {
        return ops::grayscale(image);
    }

    if (name == "brightness") {
        if (p.empty()) {
            throw std::invalid_argument("invalid pipeline stage '" + raw +
                                         "': brightness requires a value, e.g. brightness=20");
        }
        const double b = parseDouble(p, raw, "brightness");
        return ops::brightnessContrast(image, b, 1.0);
    }

    if (name == "contrast") {
        if (p.empty()) {
            throw std::invalid_argument("invalid pipeline stage '" + raw +
                                         "': contrast requires a value, e.g. contrast=1.2");
        }
        const double c = parseDouble(p, raw, "contrast");
        return ops::brightnessContrast(image, 0.0, c);
    }

    if (name == "convolve") {
        if (p == "sharpen") {
            return ops::convolve3x3(image, ops::kernels::sharpen, false);
        }
        if (p == "edge") {
            return ops::convolve3x3(image, ops::kernels::edgeDetect, false);
        }
        // Custom kernel: 9 colon-separated numbers, e.g. convolve=0:-1:0:-1:5:-1:0:-1:0
        // (colon, not comma, since commas already separate pipeline stages).
        std::array<double, 9> kernel{};
        const std::vector<std::string> nums = splitOn(p, ':');
        if (nums.size() != 9) {
            throw std::invalid_argument(
                "invalid pipeline stage '" + raw +
                "': convolve requires 'sharpen', 'edge', or 9 colon-separated numbers");
        }
        for (std::size_t i = 0; i < 9; ++i) {
            kernel[i] = parseDouble(trim(nums[i]), raw, "kernel value " + std::to_string(i));
        }
        return ops::convolve3x3(image, kernel, false);
    }

    if (name == "crop") {
        // crop=X:Y:W:H (colon, not comma, since commas already separate
        // pipeline stages).
        const std::vector<std::string> nums = splitOn(p, ':');
        if (nums.size() != 4) {
            throw std::invalid_argument("invalid pipeline stage '" + raw +
                                         "': crop requires X:Y:W:H");
        }
        const int x = parseInt(trim(nums[0]), raw, "x");
        const int y = parseInt(trim(nums[1]), raw, "y");
        const int w = parseInt(trim(nums[2]), raw, "w");
        const int h = parseInt(trim(nums[3]), raw, "h");
        return ops::crop(image, x, y, w, h);
    }

    if (name == "flip") {
        if (p == "h" || p == "horizontal") {
            return ops::flip(image, ops::FlipDirection::Horizontal);
        }
        if (p == "v" || p == "vertical") {
            return ops::flip(image, ops::FlipDirection::Vertical);
        }
        throw std::invalid_argument("invalid pipeline stage '" + raw +
                                     "': flip requires 'h' or 'v'");
    }

    if (name == "rotate") {
        if (p.empty()) {
            throw std::invalid_argument("invalid pipeline stage '" + raw +
                                         "': rotate requires a degree value, e.g. rotate=90");
        }
        const int degrees = parseInt(p, raw, "degrees");
        return ops::rotate90(image, degrees);
    }

    throw std::invalid_argument("invalid pipeline stage '" + raw + "': unknown operation '" +
                                 name + "'");
}

Image runPipeline(const Image& src, const std::string& spec) {
    Image current = src;
    for (const Stage& stage : parseOps(spec)) {
        current = applyStage(current, stage);
    }
    return current;
}

} // namespace imgpipe
