#include "lpt/proc_reader.hpp"

#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

namespace lpt {
namespace {

bool is_digits(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

}  // namespace

ProcReader::ProcReader(std::string root) : root_(std::move(root)) {
    if (root_.empty()) {
        root_ = "/";
    }
    while (root_.size() > 1 && root_.back() == '/') {
        root_.pop_back();
    }
}

std::string ProcReader::join(const std::string& relative) const {
    if (relative.empty() || relative[0] == '/') {
        return root_ + relative;
    }
    return root_ + "/" + relative;
}

bool ProcReader::exists(const std::string& relative_path) const {
    struct stat st {};
    return ::stat(join(relative_path).c_str(), &st) == 0;
}

std::string ProcReader::read_file(const std::string& relative_path) const {
    std::ifstream in(join(relative_path), std::ios::in | std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open " + join(relative_path));
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> ProcReader::read_lines(const std::string& relative_path) const {
    std::ifstream in(join(relative_path));
    if (!in) {
        throw std::runtime_error("failed to open " + join(relative_path));
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<int> ProcReader::list_pids() const {
    std::vector<int> pids;
    const std::string proc_dir = join("/proc");
    DIR* dir = ::opendir(proc_dir.c_str());
    if (dir == nullptr) {
        throw std::runtime_error("failed to open " + proc_dir);
    }
    while (const dirent* ent = ::readdir(dir)) {
        if (is_digits(ent->d_name)) {
            pids.push_back(std::atoi(ent->d_name));
        }
    }
    ::closedir(dir);
    return pids;
}

std::vector<std::string> ProcReader::split_ws(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        out.push_back(std::move(tok));
    }
    return out;
}

std::int64_t ProcReader::parse_i64(const std::string& s, std::int64_t fallback) {
    try {
        return static_cast<std::int64_t>(std::stoll(s));
    } catch (...) {
        return fallback;
    }
}

double ProcReader::parse_double(const std::string& s, double fallback) {
    try {
        return std::stod(s);
    } catch (...) {
        return fallback;
    }
}

std::map<std::string, std::int64_t> ProcReader::parse_key_value_kb(const std::string& text) {
    std::map<std::string, std::int64_t> kv;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, colon);
        std::string rest = line.substr(colon + 1);
        auto tokens = split_ws(rest);
        if (tokens.empty()) {
            continue;
        }
        kv[key] = parse_i64(tokens[0]);
    }
    return kv;
}

}  // namespace lpt
