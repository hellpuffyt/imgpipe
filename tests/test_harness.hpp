#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// A small, hand-rolled test harness instead of an external framework.
//
// Rationale: imgpipe's Docker-based CI gate builds with -Werror and
// sanitizers on a from-scratch container; pulling in Catch2 via
// FetchContent adds a network dependency (and non-trivial build time) to
// every CI run and every contributor's first build, for a feature set this
// project doesn't need. A ~60-line registry + CHECK macros gives named
// tests, per-assertion pass/fail counts, clear file:line failure output,
// and a single CTest-integrated executable -- everything the test plan
// calls for -- without an extra dependency.
namespace imgpipe::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back(TestCase{name, std::move(fn)});
    }
};

inline int& assertionCount() {
    static int c = 0;
    return c;
}

inline int& failureCount() {
    static int c = 0;
    return c;
}

inline void reportFailure(const std::string& expr, const std::string& file, int line) {
    std::cerr << file << ":" << line << ": FAILED: " << expr << "\n";
    ++failureCount();
}

} // namespace imgpipe::test

#define IMGPIPE_TEST(name)                                                                       \
    static void name();                                                                          \
    static ::imgpipe::test::Registrar registrar_##name(#name, name);                              \
    static void name()

#define CHECK(cond)                                                                               \
    do {                                                                                           \
        ++::imgpipe::test::assertionCount();                                                       \
        if (!(cond)) {                                                                             \
            ::imgpipe::test::reportFailure(#cond, __FILE__, __LINE__);                             \
        }                                                                                           \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                      \
    do {                                                                                           \
        ++::imgpipe::test::assertionCount();                                                       \
        if (!(std::abs(static_cast<double>(a) - static_cast<double>(b)) <= (tol))) {               \
            ::imgpipe::test::reportFailure(#a " ~= " #b, __FILE__, __LINE__);                      \
        }                                                                                           \
    } while (0)

#define CHECK_THROWS(expr)                                                                         \
    do {                                                                                           \
        ++::imgpipe::test::assertionCount();                                                       \
        bool imgpipe_threw = false;                                                                \
        try {                                                                                       \
            (void)(expr);                                                                            \
        } catch (...) {                                                                             \
            imgpipe_threw = true;                                                                   \
        }                                                                                            \
        if (!imgpipe_threw) {                                                                       \
            ::imgpipe::test::reportFailure(#expr " to throw", __FILE__, __LINE__);                 \
        }                                                                                            \
    } while (0)

#define CHECK_NOTHROW(expr)                                                                         \
    do {                                                                                           \
        ++::imgpipe::test::assertionCount();                                                       \
        try {                                                                                       \
            (void)(expr);                                                                            \
        } catch (const std::exception& imgpipe_ex) {                                                \
            ::imgpipe::test::reportFailure(                                                         \
                std::string(#expr) + " not to throw, but it threw: " + imgpipe_ex.what(),           \
                __FILE__, __LINE__);                                                                 \
        } catch (...) {                                                                             \
            ::imgpipe::test::reportFailure(std::string(#expr) + " not to throw", __FILE__, __LINE__); \
        }                                                                                            \
    } while (0)
