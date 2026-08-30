#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "imgpipe/bench.hpp"
#include "imgpipe/image.hpp"
#include "imgpipe/pipeline.hpp"

namespace {

void printUsage(std::ostream& out) {
    out << "imgpipe -- a small, fast image processing pipeline\n\n"
        << "Usage:\n"
        << "  imgpipe --in INPUT --out OUTPUT --ops OPS_SPEC\n"
        << "  imgpipe --in INPUT --ops OPS_SPEC --bench [--bench-out FILE.json]\n"
        << "  imgpipe --in INPUT --ops OPS_SPEC --bench --baseline FILE.json "
        << "[--threshold PERCENT]\n\n"
        << "Options:\n"
        << "  --in FILE          Input image (PNG/JPEG/BMP/TGA)\n"
        << "  --out FILE         Output image; extension selects format "
        << "(.png/.bmp/.tga/.jpg)\n"
        << "  --ops SPEC         Comma-separated pipeline stages, e.g.\n"
        << "                     \"resize=800x600,gaussian=2.0,gray\"\n"
        << "  --bench            Measure per-stage throughput instead of "
        << "writing --out\n"
        << "  --bench-out FILE   Write the benchmark report as JSON to FILE\n"
        << "  --baseline FILE    Compare the benchmark run against a stored "
        << "baseline JSON\n"
        << "  --threshold PCT    Regression threshold in percent slowdown "
        << "(default: 15)\n"
        << "  --help             Show this message\n\n"
        << "Pipeline stages:\n"
        << "  resize=WxH[:bilinear|box]  box is auto-selected when "
        << "downscaling, bilinear\n"
        << "                              when upscaling, unless a method "
        << "is given explicitly\n"
        << "  gaussian=SIGMA              separable Gaussian blur\n"
        << "  gray                        BT.709 luma-weighted grayscale\n"
        << "  brightness=DELTA            additive brightness, e.g. -50..50\n"
        << "  contrast=FACTOR             multiplicative contrast around "
        << "mid-grey, 1.0=none\n"
        << "  convolve=sharpen|edge|k0:...:k8   3x3 convolution (colon-separated "
        << "kernel)\n"
        << "  crop=X:Y:W:H                crop a rectangle\n"
        << "  flip=h|v                    flip horizontally/vertically\n"
        << "  rotate=90|180|270           rotate clockwise\n";
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open '" + path + "' for reading");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeFile(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open '" + path + "' for writing");
    }
    out << contents;
}

struct Args {
    std::string in;
    std::string out;
    std::string ops;
    bool bench = false;
    std::string benchOut;
    std::string baseline;
    double threshold = 15.0;
    bool help = false;
};

Args parseArgs(int argc, char** argv) {
    Args args;
    std::vector<std::string> tokens(argv + 1, argv + argc);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= tokens.size()) {
                throw std::invalid_argument("missing value for argument '" + tok + "'");
            }
            return tokens[++i];
        };
        if (tok == "--in") {
            args.in = next();
        } else if (tok == "--out") {
            args.out = next();
        } else if (tok == "--ops") {
            args.ops = next();
        } else if (tok == "--bench") {
            args.bench = true;
        } else if (tok == "--bench-out") {
            args.benchOut = next();
        } else if (tok == "--baseline") {
            args.baseline = next();
        } else if (tok == "--threshold") {
            const std::string value = next();
            double threshold = 0.0;
            // std::from_chars is locale-independent, unlike std::stod;
            // see the comment on parseDouble() in pipeline.cpp for why
            // that matters here.
            const auto result =
                std::from_chars(value.data(), value.data() + value.size(), threshold);
            if (result.ec != std::errc() || result.ptr != value.data() + value.size()) {
                throw std::invalid_argument("invalid --threshold value '" + value + "'");
            }
            args.threshold = threshold;
        } else if (tok == "--help" || tok == "-h") {
            args.help = true;
        } else {
            throw std::invalid_argument("unknown argument '" + tok + "'");
        }
    }
    return args;
}

} // namespace

int main(int argc, char** argv) {
    try {
        Args args = parseArgs(argc, argv);
        if (args.help || argc == 1) {
            printUsage(std::cout);
            return args.help ? 0 : 1;
        }
        if (args.in.empty()) {
            std::cerr << "error: --in is required\n";
            printUsage(std::cerr);
            return 1;
        }

        imgpipe::Image src = imgpipe::loadImage(args.in);

        if (args.bench) {
            imgpipe::bench::BenchReport report = imgpipe::bench::runBenchmark(src, args.ops);

            std::cout << "Stage throughput (input megapixels/second):\n";
            for (const auto& stage : report.stages) {
                std::cout << "  " << stage.stageRaw << ": " << stage.megapixelsPerSecond
                          << " Mpix/s (" << stage.wallSeconds * 1000.0 << " ms, input "
                          << stage.inputWidth << "x" << stage.inputHeight << ")\n";
            }
            std::cout << "Total wall time: " << report.totalWallSeconds * 1000.0 << " ms\n";
            if (report.peakResidentBytes >= 0) {
                std::cout << "Peak resident memory: "
                          << (report.peakResidentBytes / (1024.0 * 1024.0)) << " MiB\n";
            } else {
                std::cout << "Peak resident memory: unavailable on this platform\n";
            }

            const std::string json = imgpipe::bench::toJson(report);
            if (!args.benchOut.empty()) {
                writeFile(args.benchOut, json);
            }

            if (!args.baseline.empty()) {
                imgpipe::bench::BenchReport baseline =
                    imgpipe::bench::parseJson(readFile(args.baseline));
                auto regressions =
                    imgpipe::bench::compareToBaseline(baseline, report, args.threshold);
                if (!regressions.empty()) {
                    std::cerr << "Performance regression detected (threshold "
                              << args.threshold << "%):\n";
                    for (const auto& r : regressions) {
                        std::cerr << "  " << r.stageRaw << ": " << r.baselineMpps
                                  << " -> " << r.currentMpps << " Mpix/s ("
                                  << r.percentSlower << "% slower)\n";
                    }
                    return 2;
                }
                std::cout << "No regressions beyond " << args.threshold << "% threshold.\n";
            }
            return 0;
        }

        if (args.out.empty()) {
            std::cerr << "error: --out is required unless --bench is given\n";
            return 1;
        }
        imgpipe::Image result = imgpipe::runPipeline(src, args.ops);
        imgpipe::saveImage(args.out, result);
        std::cout << "Wrote " << args.out << " (" << result.width() << "x" << result.height()
                  << ", " << result.channels() << " channels)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
