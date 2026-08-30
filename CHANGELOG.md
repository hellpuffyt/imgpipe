# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-08-30

### Added

- Core `Image` type: dense, row-major, RAII pixel buffer supporting 1/2/3/4
  channel images.
- PNG/BMP/TGA/JPEG loading and saving via vendored `stb_image` / `stb_image_write`.
- Image operations: bilinear and box (area) resize, separable Gaussian blur,
  BT.709-weighted grayscale, brightness/contrast, generic 3x3 convolution
  (with named sharpen/edge-detect kernels), crop, flip, and 90-degree-multiple
  rotation.
- Command-line pipeline (`--ops`) chaining operations with validated syntax
  and descriptive error messages.
- Throughput benchmarking (`--bench`) reporting per-stage megapixels/second,
  wall time, and peak resident memory, with JSON baseline comparison and
  regression detection (`--baseline`, `--threshold`).
- Test suite (hand-rolled harness + CTest) covering every operation,
  including a direct-2D-convolution cross-check of the separable Gaussian
  blur and box-vs-bilinear downscale aliasing behavior.
- CI: build/test on Ubuntu and macOS with GCC and Clang, a dedicated
  `-Werror` gate, and an AddressSanitizer + UndefinedBehaviorSanitizer gate.
