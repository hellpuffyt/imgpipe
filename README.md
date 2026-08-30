# imgpipe

A small, dependency-light C++20 image processing pipeline: resize, blur,
convert, and chain operations from the command line -- and measure exactly
how fast each stage runs, so a change that makes it slower is visible in CI
rather than discovered in production.

## What

`imgpipe` reads a PNG/JPEG/BMP/TGA image, runs it through a chain of
operations you specify on the command line, and writes the result back out.
Each operation is implemented from scratch against `std::vector<uint8_t>` --
no OpenCV, no ImageMagick, no Boost. The only third-party code is the
single-header `stb_image` / `stb_image_write` decoders (see
[Third-party licenses](#third-party-licenses)); every actual image
transform -- resampling, convolution, color math -- is this project's own
code.

```
imgpipe --in photo.png --out thumb.png \
  --ops "resize=800x600,gaussian=1.2,gray"
```

## Why

Most small image tools fall into one of two camps: thin wrappers around
ImageMagick (inheriting a huge dependency and its performance envelope), or
toy filter collections that don't measure themselves. imgpipe does a
deliberately small set of operations, does them correctly (bilinear vs. box
resampling, separable vs. 2D Gaussian, real luminance weights instead of a
naive average), and treats throughput as a first-class, testable property:
`--bench` reports measured megapixels/second per stage and can fail a run
that regresses against a stored baseline.

## Operations

| Stage | Syntax | Notes |
|---|---|---|
| Resize | `resize=WxH[:bilinear\|box]` | See sampling guidance below. If no method is given, box is auto-selected when either dimension shrinks, bilinear otherwise. |
| Gaussian blur | `gaussian=SIGMA` | Separable: one horizontal 1D pass, one vertical 1D pass. |
| Grayscale | `gray` | ITU-R BT.709 luma: `0.2126*R + 0.7152*G + 0.0722*B`. |
| Brightness | `brightness=DELTA` | Additive, e.g. `-50`..`50`. |
| Contrast | `contrast=FACTOR` | Multiplicative around the mid-grey pivot (127.5); `1.0` = no change. |
| Convolution | `convolve=sharpen\|edge\|k0:...:k8` | 3x3 kernel (colon-separated for a custom kernel, since commas separate pipeline stages). Border pixels are edge-clamped. |
| Crop | `crop=X:Y:W:H` | Must be fully inside the source image. |
| Flip | `flip=h\|v` | Horizontal or vertical. |
| Rotate | `rotate=90\|180\|270` | Clockwise, multiples of 90 only. |

### Resize sampling guidance

**Downscaling: use box (area) sampling.** Box sampling averages every source
pixel that contributes to a destination pixel, weighted by area of overlap.
That is the statistically correct thing to do when many source pixels
collapse onto one destination pixel, and it's what avoids moire/aliasing
artifacts. Bilinear sampling, by contrast, only looks at a 2x2 neighborhood
around a single sample point when downscaling -- it throws away most of the
source data, and the result depends heavily on exactly where that sample
point happens to land. `tests/test_resize.cpp` has a test
(`resize_box_vs_bilinear_aliasing_on_checkerboard`) that demonstrates this
directly on a high-frequency checkerboard.

**Upscaling: use bilinear sampling.** Each destination pixel maps to a
fractional position inside a single source 2x2 neighborhood, so smooth
interpolation is both correct and cheap. Box sampling degenerates to
(near) nearest-neighbor when upscaling and produces visibly blocky output.

If you don't specify a method, imgpipe picks the correct one automatically
based on whether the target is smaller or larger than the source.

## Architecture

```
include/imgpipe/
  image.hpp     Image: dense row-major uint8 buffer + PNG/BMP/TGA/JPEG I/O
  ops.hpp       Pure functions: resize, gaussianBlur, grayscale,
                brightnessContrast, convolve3x3, crop, flip, rotate90
  pipeline.hpp  Stage parsing ("--ops" spec) and sequential application
  bench.hpp     Per-stage timing, JSON report, baseline regression check

src/            Implementations (one .cpp per concern; see CMakeLists.txt)
third_party/stb/  Vendored stb_image.h / stb_image_write.h (public domain)
tests/          Hand-rolled test harness + CTest integration
```

`Image` owns a single `std::vector<std::uint8_t>` (row-major, interleaved
channels) and never uses raw `new`/`delete`. Every operation is a pure
function taking `const Image&` and returning a new `Image` -- no in-place
mutation, no aliasing hazards, no ownership questions. `pipeline.hpp` parses
the `--ops` command-line spec into a list of stages and applies them via
`ops.hpp` in order, with syntax validation that names the failing stage.
`bench.hpp` reuses the same stage list to time each one individually.

## Building

Requires CMake >= 3.16 and a C++20 compiler (GCC >= 11 or Clang >= 14).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Build options (all `-D<NAME>=ON/OFF`):

- `IMGPIPE_WARNINGS_AS_ERRORS` (default `ON`) -- `-Wall -Wextra -Wpedantic -Werror`
- `IMGPIPE_ENABLE_SANITIZERS` (default `OFF`) -- `-fsanitize=address,undefined`
- `IMGPIPE_BUILD_TESTS` (default `ON`)

Sanitizer build:

```
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DIMGPIPE_ENABLE_SANITIZERS=ON
cmake --build build-san -j
ctest --test-dir build-san --output-on-failure
```

## Usage

```
imgpipe --in INPUT --out OUTPUT --ops OPS_SPEC
imgpipe --in INPUT --ops OPS_SPEC --bench [--bench-out FILE.json]
imgpipe --in INPUT --ops OPS_SPEC --bench --baseline FILE.json [--threshold PERCENT]
```

```
# Resize and convert to grayscale
imgpipe --in photo.png --out thumb.png --ops "resize=800x600,gray"

# Blur, sharpen, and rotate
imgpipe --in photo.png --out out.png \
  --ops "gaussian=2.0,convolve=sharpen,rotate=90"

# Measure throughput
imgpipe --in photo.png --ops "resize=1920x1440:box,gaussian=2.0,gray" --bench

# Save a baseline, then fail CI if a later change regresses more than 15%
imgpipe --in photo.png --ops "resize=1920x1440:box,gaussian=2.0" --bench \
  --bench-out benchmarks/baseline.json
imgpipe --in photo.png --ops "resize=1920x1440:box,gaussian=2.0" --bench \
  --baseline benchmarks/baseline.json --threshold 15
```

`imgpipe --help` prints the full option and stage reference.

## Benchmarks

Measured with the release build (`-O3`) inside the project's `gcc:14` Docker
build container, on a synthetic 4000x3000 RGB gradient image, running the
pipeline `resize=1920x1440:box,gaussian=2.0,gray,brightness=10,contrast=1.1,convolve=sharpen,flip=h,rotate=90`:

| Stage | Input size | Time | Throughput |
|---|---|---|---|
| `resize=1920x1440:box` | 4000x3000 | 127.3 ms | 94.3 Mpix/s |
| `gaussian=2.0` | 1920x1440 | 193.8 ms | 14.3 Mpix/s |
| `gray` | 1920x1440 | 7.6 ms | 364.6 Mpix/s |
| `brightness=10` | 1920x1440 | 9.6 ms | 287.9 Mpix/s |
| `contrast=1.1` | 1920x1440 | 9.2 ms | 299.7 Mpix/s |
| `convolve=sharpen` | 1920x1440 | 20.5 ms | 135.0 Mpix/s |
| `flip=h` | 1920x1440 | 2.1 ms | 1287.4 Mpix/s |
| `rotate=90` | 1920x1440 | 2.1 ms | 1318.3 Mpix/s |

Total wall time: 373.6 ms. Peak resident memory: 80.1 MiB.

These numbers are what one Docker container run produced, not aspirational
figures -- expect variance run to run and machine to machine (a second run
against the same baseline in the same container showed several stages 15-30%
off from these, purely from container scheduling noise, not a real
regression -- see the note on `--threshold` below). The Gaussian blur is
deliberately the slowest per-pixel stage here (radius scales with sigma, and
this run uses sigma=2.0, i.e. a 13-pixel-wide 1D kernel run twice); that's
the separable algorithm doing real work, not a regression.

**How a stage is timed.** Each stage is timed individually with a monotonic
clock. If a single application completes in well under a couple of
milliseconds -- a small image, a cheap stage, a fast machine, or just a
clock with coarse resolution -- that one sample is dominated by measurement
noise, not the stage's actual cost. So imgpipe re-applies the stage
(discarding all but the last result, to keep the pipeline's output correct)
until the *cumulative* elapsed time clears a 2 ms floor, and reports the
per-application average over that batch. `wallSeconds` and
`megapixelsPerSecond` in the JSON report are therefore always an average
over one or more real, summed measurements, never a single sub-tick sample.
If even that fails to clear the floor, the stage's throughput is reported as
`null` in JSON (there is no JSON infinity/NaN literal) and printed as
`(unmeasurable)` on the console -- never as a fabricated number, and never
as "infinitely fast". `compareToBaseline` skips a stage on either side of
the comparison when its throughput is unmeasurable, rather than comparing
against, or reporting a regression against, a number that isn't real.

Use `--bench --bench-out` to capture your own baseline and `--baseline` to
gate future changes against it; because run-to-run variance on a shared or
virtualized machine (like a CI runner) can easily exceed 15-20% on fast,
small stages, pick `--threshold` with that noise floor in mind rather than
treating it as a tight tolerance.

## Testing

```
ctest --test-dir build --output-on-failure
```

64 test cases, 1506 assertions, run via a small hand-rolled harness (see
[Why not Catch2](#why-a-hand-rolled-test-harness) below) driven through a
single CTest entry point. Coverage includes:

- A known horizontal gradient resized to a known target size produces the
  exact expected interpolated/averaged values.
- Box-downscaling a 1px-cell checkerboard by exactly 2x averages to exactly
  mid-grey (127.5, rounds to 128); a finer checkerboard downscaled by a
  non-integer factor shows bilinear deviating from mid-grey more than box
  does, demonstrating the aliasing box sampling avoids.
- The separable two-pass Gaussian blur is checked pixel-by-pixel against an
  unoptimized direct 2D convolution using the same kernel, within +/-1
  intensity level -- the test that proves the O(N*radius) optimization is
  numerically correct, not just fast.
- Grayscale of a pure red/green/blue pixel matches the BT.709 weights
  exactly (e.g. red -> `0.2126 * 255`), and is checked to differ from a
  naive `(R+G+B)/3` average.
- Rotating 90 degrees four times returns the original image exactly
  (bit-for-bit).
- A 1x1 image survives every single operation (resize up/down, blur,
  grayscale, brightness/contrast, convolution, crop, flip, rotate) without
  crashing or misbehaving.
- Extreme aspect ratio resizes (e.g. 100x1 -> 20x50) and malformed
  `--ops` syntax (missing values, non-numeric arguments, unknown stage
  names, out-of-bounds crops) are covered explicitly.
- `bench.hpp`'s JSON serialization round-trips exactly, and its baseline
  comparison correctly flags a synthetic regression above threshold and
  ignores one below it.

CI additionally runs the full suite under AddressSanitizer +
UndefinedBehaviorSanitizer -- the check that matters most for image code
doing per-pixel index arithmetic -- and a dedicated `-Werror` build.

### Why a hand-rolled test harness

Catch2 (via CMake `FetchContent`) was considered, but pulling it in adds a
network dependency and extra build time to every CI run and every
contributor's first build, for features (BDD-style sections, matchers,
fixtures) this project's flat, synthetic-image-based test suite doesn't
need. The harness in `tests/test_harness.hpp` is under 90 lines: a
self-registering `IMGPIPE_TEST` macro, `CHECK`/`CHECK_NEAR`/`CHECK_THROWS`
assertions with file:line failure reporting, and a single `main()` that
runs everything and returns a CTest-friendly exit code. That's everything
the test plan above needed.

## Third-party licenses

This project vendors two single-header libraries from
[nothings/stb](https://github.com/nothings/stb) under `third_party/stb/`:

- `stb_image.h` -- image loading (PNG/JPEG/BMP/TGA/etc.)
- `stb_image_write.h` -- image writing (PNG/BMP/TGA/JPEG)

Both are dual-licensed by their author (Sean Barrett and contributors)
under your choice of the MIT License or the Unlicense (public domain); the
full license text is preserved at the top of each vendored file. No
modifications have been made to either file beyond the two `#define
STB_IMAGE*_IMPLEMENTATION` lines that select translation-unit-local
compilation, which live in `src/image.cpp`, not in the vendored files
themselves.

## Security

- No network access, no dynamic code execution, no shelling out.
- All pixel and buffer indexing goes through bounds-checked (`Image::at`)
  or explicitly-derived (`Image::pixel`, only ever called with
  loop-invariant-bounded indices) accessors; the full test suite is run
  under AddressSanitizer + UndefinedBehaviorSanitizer in CI specifically to
  catch any indexing mistake in the latter category.
- Command-line pipeline parsing rejects malformed input with a descriptive
  error rather than guessing; it never executes or evaluates the spec text
  as code.
- Decoding untrusted image files still goes through `stb_image`, a
  widely-used but not formally hardened decoder; don't feed it files from
  untrusted sources in a security-sensitive context without additional
  sandboxing.

## License

MIT. See [LICENSE](LICENSE).
