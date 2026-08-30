#include <clocale>
#include <cstdio>
#include <string>

#include "imgpipe/pipeline.hpp"
#include "test_fixtures.hpp"
#include "test_harness.hpp"

using namespace imgpipe;
using namespace imgpipe::test;

namespace {

// RAII guard that restores the process's C locale on scope exit, so this
// test can never leak a locale change into whichever test runs next.
class ScopedCLocale {
public:
    ScopedCLocale() {
        const char* current = std::setlocale(LC_ALL, nullptr);
        previous_ = current != nullptr ? current : "C";
    }
    ~ScopedCLocale() { std::setlocale(LC_ALL, previous_.c_str()); }

    // Returns true if `name` could actually be activated.
    bool tryActivate(const char* name) { return std::setlocale(LC_ALL, name) != nullptr; }

private:
    std::string previous_;
};

} // namespace

IMGPIPE_TEST(pipeline_numeric_parsing_is_locale_independent) {
    // Regression test for a real bug: pipeline.cpp used to parse numeric
    // stage parameters ("gaussian=2.0", "contrast=1.1", ...) with
    // std::stod/std::stoi, which defer to the C library's strtod/strtol
    // and therefore to the process's *current C locale*. Under a locale
    // where ',' is the decimal separator (e.g. de_DE), std::stod("2.0")
    // stops consuming at the '.', and this code's "was the whole string
    // consumed?" check then threw std::invalid_argument for perfectly
    // valid, locale-neutral command-line input. Because that throw
    // happened inside ordinary pipeline application code (not behind any
    // CHECK_THROWS), it escaped the test harness entirely and aborted the
    // whole test binary before a single result could be printed. Parsing
    // now goes through std::from_chars, which the standard requires to be
    // locale-independent, so this must hold under any locale.
    ScopedCLocale guard;

    bool activatedNonDotLocale = false;
    for (const char* candidate : {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "de_DE.ISO8859-1"}) {
        if (guard.tryActivate(candidate)) {
            activatedNonDotLocale = true;
            break;
        }
    }

    Image src = makeSolid(4, 4, 1, 100);
    CHECK_NOTHROW(runPipeline(src, "gaussian=2.0,brightness=1.5,contrast=1.25,resize=2x2:box"));

    // If we couldn't switch to a comma-decimal locale on this machine, the
    // assertion above still ran (under whatever locale is default here),
    // it just isn't the strongest possible reproduction of the original
    // bug. Surface that clearly rather than claiming full coverage.
    if (!activatedNonDotLocale) {
        std::fprintf(stderr,
                      "note: pipeline_numeric_parsing_is_locale_independent could not activate a "
                      "comma-decimal locale (e.g. de_DE.UTF-8) on this machine; ran under the "
                      "default locale instead.\n");
    }
}
