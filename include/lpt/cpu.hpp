#pragma once

#include "lpt/proc_reader.hpp"
#include "lpt/types.hpp"

namespace lpt {

CpuSnapshot parse_proc_stat(const std::string& text);
CpuSnapshot collect_cpu(const ProcReader& reader);
CpuRates compute_cpu_rates(const CpuSnapshot& prev, const CpuSnapshot& cur, double elapsed_sec);

}  // namespace lpt
