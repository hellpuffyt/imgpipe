# Contributing

Thanks for considering a contribution to imgpipe.

## Building

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Ground rules

- C++20, no compiler extensions.
- The default build treats warnings as errors (`-Wall -Wextra -Wpedantic -Werror`
  on GCC/Clang). Code that doesn't compile warning-free won't be merged.
- No raw `new`/`delete`. Use `std::vector` and other RAII containers.
- No new third-party dependencies beyond what's already vendored, without
  discussion first. The project intentionally stays dependency-light.
- Every new operation needs tests: at minimum, a known-value case, a 1x1
  edge case, and a boundary/error case (invalid arguments should throw
  `std::invalid_argument` with a message that names the problem).
- If you touch a hot path (resize, blur, convolution), run the sanitizer
  build before opening a PR:

  ```
  cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DIMGPIPE_ENABLE_SANITIZERS=ON
  cmake --build build-san -j
  ctest --test-dir build-san --output-on-failure
  ```

- If you touch a hot path, also re-run `--bench` and compare against
  `benchmarks/baseline.json` (see the README's Benchmarks section) so
  regressions are caught before merge, not after.

## Commit style

Small, focused commits with a plain-language summary of *why*, not just
*what*, are preferred.

## Reporting issues

Please include: the exact command you ran, the input image dimensions and
channel count, and the full error message. For anything involving numeric
output, the expected vs. actual pixel values are extremely helpful.
