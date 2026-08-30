#include <iostream>

#include "test_harness.hpp"

int main() {
    int ran = 0;
    for (const auto& test : imgpipe::test::registry()) {
        test.fn();
        ++ran;
    }
    const int assertions = imgpipe::test::assertionCount();
    const int failures = imgpipe::test::failureCount();
    std::cout << "imgpipe tests: " << ran << " test cases, " << assertions
              << " assertions, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
