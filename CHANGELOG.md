# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- `crop=X:Y:W:H` with values near `INT_MAX` overflowed `x + w` / `y + h`,
  passed the bounds check and read out of bounds (SIGBUS in a release build,
  reported by UBSan). The check no longer overflows.
- `gaussian=SIGMA` with `nan`, `inf` or a huge sigma converted a non-finite
  or out-of-range double to `int` (undefined behaviour). Every numeric stage
  parameter must now be finite, `--threshold` must be a finite value `>= 0`
  (a `nan` threshold made the regression gate always pass), and sigma is
  capped at 1000.
- A tiny image file whose header declared enormous dimensions was "decoded"
  into gigabytes of memory (stb_image pads missing rows); found by fuzzing a
  2.6 KB BMP that allocated about 2 GB. Headers are now checked before
  decoding and images above 256 megapixels are rejected.
- `Image` dimensions are validated before allocating (a negative size
  previously surfaced as `std::bad_alloc`/`vector` errors), images whose
  buffer would exceed 2 GiB are rejected with a clear message, and pixel
  offsets are computed in `std::size_t`.
- stb_image's JPEG IDCT overflowed signed `int` on a corrupt file (UBSan);
  `src/image.cpp` is now compiled with `-fwrapv`, which makes that arithmetic
  defined without modifying the vendored header.
- `--baseline` with a baseline recorded for a different `--ops` pipeline (or
  an empty one) was compared stage-by-position or silently skipped; it is now
  an error that names the mismatching stage.

### Security

- Only the PNG, JPEG, BMP and TGA decoders are compiled into the binary
  (`STBI_ONLY_*`), matching the documented formats; GIF, PSD, HDR, PIC and
  PNM input is now rejected.

### Added

- `tests/test_hardening.cpp`: truncated and corrupted inputs for every
  supported format, oversized headers, overflow and non-finite parameter
  regressions. `SECURITY.md`, `.editorconfig`, `docs/PRODUCTION_READINESS.md`.

### Fixed (earlier)

- Pipeline stage numeric parsing (`gaussian=2.0`, `contrast=1.1`, `--threshold`,
  and the benchmark JSON reader) used `std::stod`/`std::stoi`, which defer to
  the C library's `strtod`/`strtol` and are therefore sensitive to the
  process's current C locale. Under a locale where `,` is the decimal
  separator, parsing a value like `2.0` stopped at the `.` and the
  "was the whole string consumed" check then threw for perfectly valid
  input -- on at least one CI configuration this happened outside any
  `CHECK_THROWS`, in ordinary pipeline application code, and crashed the
  entire test binary before any output was printed. Parsing now goes
  through `std::from_chars`, which is locale-independent by design; added
  a regression test that exercises the parser under a comma-decimal locale.
- `stb_image_write.h` uses `sprintf`, which some platform SDKs mark
  deprecated; the existing scoped warning relaxation for the vendored-code
  translation unit now also covers `-Wno-deprecated-declarations`, so it no
  longer trips a `-Werror` build on those platforms without weakening
  warnings anywhere else in the project.
- A stage that measured (or was constructed with) zero wall-clock time was
  encoded as throughput = +infinity; `toJson()` wrote that out via
  `operator<<` as the bare word `inf`, which is not valid JSON, and
  `parseJson()`'s number scanner could only ever match the first letter of
  that token, so it threw `malformed number 'i'` -- meaning a benchmark
  report the tool itself produced could not always be read back by the
  tool. Fixed in two parts: (1) a stage is now re-applied until cumulative
  measured time clears a 2 ms floor and the result is averaged over that
  batch, so "too fast to measure" is rare in practice; (2) when throughput
  genuinely cannot be measured, it is encoded as JSON `null` (there is no
  JSON infinity/NaN literal) rather than as a literal that isn't valid
  JSON, and `parseJson()` reads `null` back as a non-finite value
  correctly. `compareToBaseline` now also skips a stage when the *current*
  run's throughput is non-finite (it already skipped on a non-finite
  baseline), keeping the two encodings coherent. Added round-trip
  regression tests, including one that reproduces the exact original
  failure (a zero-duration stage through `toJson`/`parseJson`).

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
