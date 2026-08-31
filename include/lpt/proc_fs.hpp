#pragma once

#include <string>
#include <vector>

namespace lpt {

// Filesystem helper that can point at a real Linux root ("/") or a fixture tree
// used by unit tests (e.g. tests/fixtures/proc).
class ProcFs {
public:
    explicit ProcFs(std::string root = "/");

    const std::string& root() const { return root_; }

    std::string read_file(const std::string& relative) const;
    std::vector<std::string> list_numeric_dirs(const std::string& relative) const;
    bool exists(const std::string& relative) const;

private:
    std::string join(const std::string& relative) const;
    std::string root_;
};

}  // namespace lpt
