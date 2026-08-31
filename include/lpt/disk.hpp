#pragma once

#include "lpt/proc_reader.hpp"
#include "lpt/types.hpp"

#include <vector>

namespace lpt {

std::vector<DiskCounters> parse_diskstats(const std::string& text);
std::vector<DiskCounters> collect_disk(const ProcReader& reader);
std::vector<DiskRates> compute_disk_rates(const std::vector<DiskCounters>& prev,
                                          const std::vector<DiskCounters>& cur,
                                          double elapsed_sec);

}  // namespace lpt
