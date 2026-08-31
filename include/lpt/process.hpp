#pragma once

#include "lpt/proc_reader.hpp"
#include "lpt/types.hpp"

#include <vector>

namespace lpt {

ProcessSample parse_proc_pid_stat(int pid, const std::string& stat_text);
void enrich_from_status(ProcessSample& sample, const std::string& status_text);
void enrich_from_io(ProcessSample& sample, const std::string& io_text);

std::vector<ProcessSample> collect_processes(const ProcReader& reader, std::size_t max_procs = 256);
std::vector<ProcessRates> compute_process_rates(const std::vector<ProcessSample>& prev,
                                                const std::vector<ProcessSample>& cur,
                                                double elapsed_sec, long clk_tck,
                                                long page_size);

}  // namespace lpt
