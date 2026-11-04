// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include "hanji/version.hpp"

TEST_CASE("version matches the CMake project version") {
    CHECK(hanji::version() == HANJI_EXPECTED_VERSION);
}
