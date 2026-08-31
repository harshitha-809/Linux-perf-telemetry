#pragma once

#include "lpt/proc_reader.hpp"
#include "lpt/types.hpp"

namespace lpt {

MemoryInfo parse_meminfo(const std::string& text);
MemoryInfo collect_memory(const ProcReader& reader);
double memory_used_ratio(const MemoryInfo& m);

}  // namespace lpt
