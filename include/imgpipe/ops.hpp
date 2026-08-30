#pragma once

#include <array>
#include <cstdint>

#include "imgpipe/image.hpp"

namespace imgpipe::ops {

enum class ResizeMethod { Bilinear, Box };

// Resizes `src` to newWidth x newHeight.
//
// Guidance: use Box (area) sampling when downscaling -- it averages every
// source pixel that contributes to a destination pixel, which is the
// statistically correct thing to do and avoids the aliasing/moire artifacts
// that bilinear sampling produces when many source pixels collapse onto one
// destination pixel. Use Bilinear when upscaling -- there each destination
// pixel maps to a fractional position inside a single 2x2 source
// neighbourhood, so smooth interpolation is both correct and cheap; box
// sampling degenerates to (near) nearest-neighbour there and gives blocky
// results.
Image resize(const Image& src, int newWidth, int newHeight, ResizeMethod method);

// Separable Gaussian blur: one 1D horizontal pass followed by one 1D
// vertical pass, each O(N * radius). Mathematically equivalent to a full 2D
// convolution with the outer product of the two kernels, but O(radius)
// instead of O(radius^2) per pixel. sigma must be > 0.
Image gaussianBlur(const Image& src, double sigma);

// Converts to grayscale using ITU-R BT.709 luma weights
// (0.2126 R + 0.7152 G + 0.0722 B), not a naive average. RGB input produces
// a 1-channel output; RGBA input produces a 2-channel (gray, alpha) output;
// already-gray input passes through unchanged.
Image grayscale(const Image& src);

// Adjusts brightness (additive, typically [-255, 255]) and contrast
// (multiplicative factor around the mid-grey pivot of 127.5; 1.0 = no
// change). Applied as: out = clamp((in - 127.5) * contrast + 127.5 + brightness).
// Alpha channels, if present, are left untouched.
Image brightnessContrast(const Image& src, double brightness, double contrast);

// Applies a 3x3 convolution kernel to every color channel (alpha is left
// untouched). Border pixels are handled by clamping (edge-replicate).
// If normalize is true, the kernel is divided by its sum (if nonzero) before
// being applied, which keeps overall image brightness constant -- useful for
// blur-like kernels; sharpen/edge kernels typically pass normalize=false.
Image convolve3x3(const Image& src, const std::array<double, 9>& kernel, bool normalize = false);

// Crops a rectangular region [x, y, x+w, y+h) from src. Throws
// std::invalid_argument if the rectangle is not fully contained in src.
Image crop(const Image& src, int x, int y, int w, int h);

enum class FlipDirection { Horizontal, Vertical };

Image flip(const Image& src, FlipDirection direction);

// Rotates by a multiple of 90 degrees clockwise. degrees must be one of
// 0, 90, 180, 270 (or any value congruent to one of those mod 360).
Image rotate90(const Image& src, int degrees);

// A few named 3x3 kernels usable directly with convolve3x3.
namespace kernels {
constexpr std::array<double, 9> sharpen = {0, -1, 0, -1, 5, -1, 0, -1, 0};
constexpr std::array<double, 9> edgeDetect = {-1, -1, -1, -1, 8, -1, -1, -1, -1};
constexpr std::array<double, 9> boxBlur3x3 = {1, 1, 1, 1, 1, 1, 1, 1, 1};
} // namespace kernels

} // namespace imgpipe::ops
