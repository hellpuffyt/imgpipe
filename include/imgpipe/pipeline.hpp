#pragma once

#include <string>
#include <vector>

#include "imgpipe/image.hpp"

namespace imgpipe {

// One parsed pipeline stage, e.g. "resize=800x600:box" or "gray".
struct Stage {
    std::string name;   // e.g. "resize", "gaussian", "gray", "brightness",
                         // "contrast", "convolve", "crop", "flip", "rotate"
    std::string params; // raw text after '=' (empty if the op takes no params)
    std::string raw;     // original "name=params" text, for error messages
};

// Splits a comma-separated "--ops" spec into stages. Does not validate stage
// semantics; call applyStage (or runPipeline) for that. Throws
// std::invalid_argument on malformed syntax (e.g. empty stage names).
std::vector<Stage> parseOps(const std::string& spec);

// Applies a single parsed stage to `image`, returning the result. Throws
// std::invalid_argument with a message naming the failing stage and why.
Image applyStage(const Image& image, const Stage& stage);

// Parses and applies every stage in `spec`, in order, to `src`.
Image runPipeline(const Image& src, const std::string& spec);

} // namespace imgpipe
