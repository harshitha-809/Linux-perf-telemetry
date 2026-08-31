#include "lpt/proc_fs.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <filesystem>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace lpt {
namespace {

std::string normalize_root(std::string root) {
    if (root.empty()) {
        root = "/";
    }
    while (root.size() > 1 && (root.back() == '/' || root.back() == '\\')) {
        root.pop_back();
    }
    return root;
}

}  // namespace

ProcFs::ProcFs(std::string root) : root_(normalize_root(std::move(root))) {}

std::string ProcFs::join(const std::string& relative) const {
    if (relative.empty()) {
        return root_;
    }
    std::string rel = relative;
    for (char& c : rel) {
        if (c == '\\') {
            c = '/';
        }
    }
    if (rel.front() == '/') {
        rel.erase(rel.begin());
    }
    return root_ + "/" + rel;
}

std::string ProcFs::read_file(const std::string& relative) const {
    const std::string path = join(relative);
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool ProcFs::exists(const std::string& relative) const {
#if defined(_WIN32)
    std::error_code ec;
    return std::filesystem::exists(join(relative), ec);
#else
    struct stat st {};
    return ::stat(join(relative).c_str(), &st) == 0;
#endif
}

std::vector<std::string> ProcFs::list_numeric_dirs(const std::string& relative) const {
    std::vector<std::string> out;
#if defined(_WIN32)
    std::error_code ec;
    const auto dir = std::filesystem::path(join(relative));
    if (!std::filesystem::exists(dir, ec)) {
        return out;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (!name.empty() && name.find_first_not_of("0123456789") == std::string::npos) {
            out.push_back(name);
        }
    }
#else
    DIR* d = ::opendir(join(relative).c_str());
    if (!d) {
        return out;
    }
    while (const dirent* ent = ::readdir(d)) {
        if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN) {
            continue;
        }
        const std::string name = ent->d_name;
        if (name.empty() || name[0] == '.') {
            continue;
        }
        if (name.find_first_not_of("0123456789") == std::string::npos) {
            out.push_back(name);
        }
    }
    ::closedir(d);
#endif
    return out;
}

}  // namespace lpt
