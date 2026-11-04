// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace hanji_test {

inline std::string read_fixture(const std::string& relative) {
    const std::string path = std::string(HANJI_SOURCE_DIR) + "/core/tests/fixtures/" + relative;
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open fixture " + path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace hanji_test
