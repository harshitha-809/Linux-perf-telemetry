#pragma once

#include "lpt/proc_reader.hpp"
#include "lpt/types.hpp"

namespace lpt {

VmstatCounters parse_vmstat(const std::string& text);
VmstatCounters collect_vmstat(const ProcReader& reader);
VmstatRates compute_vmstat_rates(const VmstatCounters& prev, const VmstatCounters& cur,
                                 double elapsed_sec);

}  // namespace lpt
