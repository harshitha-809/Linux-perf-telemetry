#pragma once

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace lpt {

inline std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

inline std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string tok;
    while (in >> tok) {
        out.push_back(tok);
    }
    return out;
}

inline bool starts_with(const std::string& s, const std::string& pfx) {
    return s.size() >= pfx.size() && s.compare(0, pfx.size(), pfx) == 0;
}

}  // namespace lpt
