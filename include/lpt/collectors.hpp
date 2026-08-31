#pragma once

#include "lpt/proc_fs.hpp"
#include "lpt/types.hpp"

#include <string>

namespace lpt {

CpuSnapshot parse_proc_stat(const std::string& text);
CpuSnapshot collect_cpu(const ProcFs& fs);

MemorySnapshot parse_meminfo(const std::string& text);
MemorySnapshot collect_memory(const ProcFs& fs);

VmSnapshot parse_vmstat(const std::string& text);
VmSnapshot collect_vmstat(const ProcFs& fs);

DiskSnapshot parse_diskstats(const std::string& text, bool skip_ram_and_loop = true);
DiskSnapshot collect_disk(const ProcFs& fs);

ProcessSample parse_proc_pid_stat(int pid, const std::string& text);
void apply_status_fields(ProcessSample& sample, const std::string& status_text);
void apply_io_fields(ProcessSample& sample, const std::string& io_text);
ProcessSnapshot collect_processes(const ProcFs& fs, std::size_t limit);

SystemSnapshot collect_system(const ProcFs& fs, std::size_t process_limit);

}  // namespace lpt
