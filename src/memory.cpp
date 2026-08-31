#include "lpt/memory.hpp"

namespace lpt {
namespace {

std::uint64_t get_u64(const std::map<std::string, std::int64_t>& kv, const char* key) {
    auto it = kv.find(key);
    if (it == kv.end() || it->second < 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(it->second);
}

}  // namespace

MemoryInfo parse_meminfo(const std::string& text) {
    const auto kv = ProcReader::parse_key_value_kb(text);
    MemoryInfo m;
    m.mem_total_kb = get_u64(kv, "MemTotal");
    m.mem_free_kb = get_u64(kv, "MemFree");
    m.mem_available_kb = get_u64(kv, "MemAvailable");
    m.buffers_kb = get_u64(kv, "Buffers");
    m.cached_kb = get_u64(kv, "Cached");
    m.swap_total_kb = get_u64(kv, "SwapTotal");
    m.swap_free_kb = get_u64(kv, "SwapFree");
    m.anon_pages_kb = get_u64(kv, "AnonPages");
    m.mapped_kb = get_u64(kv, "Mapped");
    m.shmem_kb = get_u64(kv, "Shmem");
    m.dirty_kb = get_u64(kv, "Dirty");
    m.writeback_kb = get_u64(kv, "Writeback");
    m.slab_kb = get_u64(kv, "Slab");
    m.committed_as_kb = get_u64(kv, "Committed_AS");
    return m;
}

MemoryInfo collect_memory(const ProcReader& reader) {
    return parse_meminfo(reader.read_file("/proc/meminfo"));
}

double memory_used_ratio(const MemoryInfo& m) {
    if (m.mem_total_kb == 0) {
        return 0.0;
    }
    const std::uint64_t avail =
        m.mem_available_kb > 0 ? m.mem_available_kb : m.mem_free_kb;
    if (avail >= m.mem_total_kb) {
        return 0.0;
    }
    return static_cast<double>(m.mem_total_kb - avail) / static_cast<double>(m.mem_total_kb);
}

}  // namespace lpt
