# Production readiness — imgpipe

Audit of the `audit/production-readiness` branch. Evidence only; the
classification at the end is a proposal for the maintainer to confirm.

- Tested commit: `1c32f0a` (code at `63b409b`; `1c32f0a` only adds a
  `mkdir -p benchmarks` line to the README; this file was added in the next
  commit and changes no code)
- Date: 2026-10-22
- Host: macOS 27 (Apple Silicon), Apple clang 21, CMake 4.4.4 (Unix
  Makefiles generator; Ninja not installed).

## Scope

A C++20 command-line image pipeline: decode PNG/JPEG/BMP/TGA (vendored
`stb_image`), apply a chain of stages (`resize`, `gaussian`, `gray`,
`brightness`, `contrast`, `convolve`, `crop`, `flip`, `rotate`), write the
result (vendored `stb_image_write`), or time each stage (`--bench`) and fail
against a stored baseline. Every transform is the project's own code over a
single `std::vector<uint8_t>` per image. Tests use a hand-rolled harness
behind one CTest entry.

## Platforms and prerequisites

- CMake >= 3.16, a C++20 compiler. Verified locally with Apple clang 21 on
  macOS arm64 only.
- CI: GCC and Clang on ubuntu-latest, Clang and GCC 14 on macos-latest, a
  `-Werror` job and an ASan+UBSan job on ubuntu. GCC and Linux were **not**
  run locally (no GCC installed here).

## Clean-clone verification

Fresh clone of the branch into an empty scratch directory, README followed
literally (a small generated PNG stands in for `photo.png`):

| Command | Exit | Result |
|---|---|---|
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` + `cmake --build build -j` | 0 | warning-clean (`-Wall -Wextra -Wpedantic -Werror` is the default) |
| `ctest --test-dir build --output-on-failure` | 0 | 1/1 CTest entry; harness: 78 test cases, 2620 assertions, 0 failures |
| sanitizer build (`-DIMGPIPE_ENABLE_SANITIZERS=ON`, Debug) + `ctest` | 0 | 78 test cases, 0 failures, no ASan/UBSan reports |
| `imgpipe --in photo.png --out thumb.png --ops "resize=800x600,gray"` | 0 | `Wrote thumb.png (800x600, 1 channels)` |
| `imgpipe … --ops "gaussian=2.0,convolve=sharpen,rotate=90"` | 0 | written, dimensions rotated |
| `imgpipe … --bench`, `--bench-out benchmarks/baseline.json`, `--baseline … --threshold 15` | 0 / 0 / 0 | report printed, baseline written, gate passed |
| `imgpipe --help` | 0 | usage |

Found on the way: the README wrote the baseline into `benchmarks/` without
creating it, and `--bench-out` does not create directories; the README now
runs `mkdir -p benchmarks` first.

## Commands executed (working clone)

| Command | Exit | Outcome |
|---|---|---|
| Release build + tests, baseline `30d63e6` | 0 | 68 test cases, 1517 assertions, 0 failures (README claimed 64 / 1506) |
| ASan+UBSan build + tests, baseline | 0 | 0 failures, no reports |
| CLI edge-argument battery under ASan+UBSan, baseline | — | UBSan: `crop=2147483647:0:1:1` signed overflow in `ops_transform.cpp`; `gaussian=nan/inf/1e30` float→int overflow in `ops_blur.cpp`; `resize=100000x100000` allocated until killed |
| same `crop` in the Release build, baseline | 138 | SIGBUS: out-of-bounds read |
| malformed-image fuzzing, baseline: 785 cases per seed (truncations at ~36 offsets and 120 random header-biased mutations of PNG RGB, PNG RGBA, JPEG, BMP, TGA, plus crafted headers and GIF/PSD/HDR/PNM magic) | — | 1 UBSan report (signed overflow in stb_image JPEG IDCT); 2 BMPs declaring 37x10-12 million pixels decoded into ~2 GB RSS (29 s) |
| same fuzzing after fixes, seeds 1-3 (2355 cases) | — | 0 sanitizer reports, 0 crashes; exit codes only 0/1; one 30 s timeout under ASan for a BMP declaring 10485797x23 (241 Mpx, inside the documented 256 Mpx cap; 5.8 s / 1.8 GB RSS in Release) |
| baseline-JSON fuzzing under ASan (542 cases: truncations, mutations, 100000-deep nesting) | — | 0 sanitizer reports, exit 0/1 only |
| Release + sanitizer build + tests after fixes | 0 | 78 test cases, 2620 assertions, 0 failures; no sanitizer reports |
| history secret scan (`git grep` over every commit) | — | no hits |

ASan leak detection is not available with Apple clang on arm64; the CI
sanitizer job (Linux) runs with `detect_leaks=1`.

Dependency audit: there are no package-managed dependencies. The only
third-party code is vendored `stb_image.h` v2.30 and `stb_image_write.h`
(latest upstream releases). stb_image has a history of reported decoder bugs
upstream; this branch reduces exposure by compiling in only the four
documented decoders and checking dimensions before decoding, and exercises the
remaining decoders with truncated/corrupted inputs under sanitizers. That is
not a security audit of stb.

## Bugs found and fixed

| # | Bug | Fix | Regression test |
|---|---|---|---|
| 1 | `crop` bounds check computed `x + w` / `y + h` in `int`; huge values overflowed, passed the check and read out of bounds (SIGBUS in Release). | Overflow-free comparisons. | `crop_rejects_rectangles_that_overflow_int` |
| 2 | Non-finite or huge `gaussian` sigma was cast to `int` (UB) when sizing the kernel; `brightness=nan`, `--threshold nan` etc. were accepted (a `nan` threshold made the regression gate always pass). | Numeric stage parameters must be finite; sigma ≤ 1000; threshold finite and ≥ 0. | `gaussian_rejects_non_finite_and_huge_sigma`, `pipeline_rejects_non_finite_numbers` |
| 3 | Decompression bomb: a 2.6 KB BMP declaring 37x12255255 pixels was decoded into ~2 GB (stb pads missing rows). | Read the file once, `stbi_info_from_memory` before decoding, reject > 256 Mpx. | `load_rejects_header_declaring_huge_dimensions` |
| 4 | `Image` allocated before validating dimensions (negative sizes surfaced as `bad_alloc`/`vector`), offsets computed in `int`, no size cap (`resize=100000x100000` tried to allocate 30 GB). | Validate first, cap buffers at 2 GiB with a clear message, `size_t` offsets. | `image_rejects_oversized_or_negative_dimensions_before_allocating` |
| 5 | Signed overflow (UB) in stb_image's JPEG IDCT on a corrupt file. | `-fwrapv` for `src/image.cpp` only; the vendored header is unmodified. | `load_corrupted_files_never_crash` (under the sanitizer build) |
| 6 | All stb decoders (GIF, PSD, HDR, PIC, PNM) were compiled in although only PNG/JPEG/BMP/TGA are documented. | `STBI_ONLY_*`. | `load_rejects_formats_that_are_not_documented` |
| 7 | `--baseline` recorded for a different pipeline was compared stage-by-position; an empty or shorter baseline silently checked nothing. | Stage count and names must match, else an error naming the stage. | `bench_compareToBaseline_rejects_mismatched_pipeline` |
| 8 | No test loaded anything but a PNG round trip. | Round trip for every format, plus truncation (every prefix up to 64 bytes, then 64 steps) and 150 deterministic corruptions per format. | `load_roundtrip_every_supported_format`, `load_truncated_files_never_crash`, `load_corrupted_files_never_crash` |

## Limitations and unresolved risks

- Memory bound is coarse: a legitimate image up to 256 Mpx is decoded (up to
  ~1 GiB for RGBA), and each pipeline stage allocates a new image.
- stb_image is a widely used but not hardened decoder; the README still
  advises sandboxing for untrusted input.
- The README benchmark table was measured by the author in a `gcc:14` Docker
  container; it was not re-measured here (different hardware, no Docker run),
  so those numbers are unverified by this audit.
- GCC, Linux and the CI `-Werror` / sanitizer jobs have not run on this branch.

## CI status

- Default branch (`30d63e6`): success.
- Audit branch: pending until pushed. CI now declares
  `permissions: contents: read`; action references are tag pins (`@v4`),
  left as they were.

## Acceptance checklist

| Item | Expected behavior | Command / test | Result | Actual | Reference |
|---|---|---|---|---|---|
| Build | warning-clean with `-Werror` | clean-clone build | pass (clang) | exit 0 | CMakeLists.txt |
| Unit tests | all pass | `ctest --test-dir build` | pass | 78 cases, 2620 assertions | tests/ |
| Sanitizers | no ASan/UBSan reports | sanitizer build + ctest | pass | 0 reports | tests/test_hardening.cpp |
| Malformed images | truncated/corrupted PNG/JPEG/BMP/TGA never crash | fuzz harness (2355 cases) + hardening tests | pass | exit 0/1 only | bugs 3, 5, 6, 8 |
| Decompression bombs | huge declared dimensions rejected from the header | `load_rejects_header_declaring_huge_dimensions` | pass | throws | bug 3 |
| Arithmetic safety | crop/resize/sigma extremes fail with an error | hardening tests, CLI battery | pass | exit 1, clear message | bugs 1, 2, 4 |
| README quick start | documented commands work | clean clone | pass | all exit 0 | README |
| Bench gate | regression above threshold fails; mismatched baseline errors | `test_bench.cpp`, CLI | pass | exit 1 with stage named | bug 7 |
| Resize correctness | box/bilinear expected values, aliasing demo | `test_resize.cpp` | pass | — | unchanged |
| Blur correctness | separable equals 2D reference ±1 | `test_blur.cpp` | pass | — | unchanged |

## Classification proposal

**Partially Verified**: the documented scope builds warning-clean and passes
all tests, plain and under ASan/UBSan, on macOS/clang, and malformed-input
handling was fuzzed. GCC/Linux CI legs have not run on this branch, the README
benchmark figures were not reproduced, and stb_image remains an unaudited
decoder for untrusted input.
