#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lpt {

// Reads Linux /proc and /sys text files. `root` is normally "/" so paths
// resolve to /proc/...; tests pass a fixture directory instead.
class ProcReader {
public:
    explicit ProcReader(std::string root = "/");

    const std::string& root() const { return root_; }

    std::string read_file(const std::string& relative_path) const;
    std::vector<std::string> read_lines(const std::string& relative_path) const;
    bool exists(const std::string& relative_path) const;
    std::vector<int> list_pids() const;

    static std::vector<std::string> split_ws(const std::string& line);
    static std::int64_t parse_i64(const std::string& s, std::int64_t fallback = 0);
    static double parse_double(const std::string& s, double fallback = 0.0);

    // Parse "Key:   123 kB" style /proc/meminfo and /proc/*/status lines.
    static std::map<std::string, std::int64_t> parse_key_value_kb(const std::string& text);

private:
    std::string join(const std::string& relative) const;
    std::string root_;
};

}  // namespace lpt
