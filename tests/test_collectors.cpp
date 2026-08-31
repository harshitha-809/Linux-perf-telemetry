#include "lpt/collectors.hpp"
#include "lpt/exporter.hpp"
#include "lpt/proc_fs.hpp"
#include "lpt/prometheus.hpp"
#include "lpt/types.hpp"

#include <gtest/gtest.h>

#include <string>

#ifndef LPT_FIXTURE_DIR
#define LPT_FIXTURE_DIR "tests/fixtures/proc"
#endif

namespace {

const std::string kStat = R"(cpu  100 10 50 800 40 5 15 2 0 0
cpu0 60 5 30 400 20 3 8 1 0 0
cpu1 40 5 20 400 20 2 7 1 0 0
ctxt 123456
btime 1700000000
processes 999
procs_running 3
procs_blocked 1
)";

const std::string kStat2 = R"(cpu  200 20 100 900 50 10 20 4 0 0
cpu0 120 10 60 450 25 6 10 2 0 0
cpu1 80 10 40 450 25 4 10 2 0 0
ctxt 123556
processes 1001
procs_running 4
procs_blocked 0
)";

const std::string kMeminfo = R"(MemTotal:        16384000 kB
MemFree:          4096000 kB
MemAvailable:    10240000 kB
Buffers:           512000 kB
Cached:           2048000 kB
SwapTotal:        2097152 kB
SwapFree:         1048576 kB
AnonPages:        3145728 kB
Mapped:            256000 kB
Shmem:             128000 kB
Dirty:               1024 kB
Writeback:              0 kB
)";

const std::string kVmstat = R"(nr_dirty 12
nr_writeback 0
pgpgin 8000
pgpgout 4000
pswpin 10
pswpout 20
pgfault 555000
pgmajfault 1200
numa_hit 100
numa_miss 2
)";

const std::string kDiskstats = R"(   8       0 sda 100 1 8000 50 200 2 16000 80 0 90 130
   8       1 sda1 50 0 4000 20 100 1 8000 40 0 40 60
   7       0 loop0 0 0 0 0 0 0 0 0 0 0 0
 259       0 nvme0n1 10 0 80 5 20 0 160 8 0 10 13
 259       1 nvme0n1p1 5 0 40 2 10 0 80 4 0 5 6
)";

const std::string kPidStat =
    "42 (lpt-agent) S 1 42 42 0 -1 4194304 100 0 3 0 25 10 0 0 20 0 4 0 12345 "
    "109051904 2048 18446744073709551615 0 0 0 0 0 0 0 0 0 0 0 17 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n";

const std::string kStatus = R"(Name:	lpt-agent
voluntary_ctxt_switches:	77
nonvoluntary_ctxt_switches:	9
)";

const std::string kIo = R"(rchar: 1000
wchar: 2000
read_bytes: 4096
write_bytes: 8192
)";

}  // namespace

TEST(CpuParser, AggregateAndPerCpu) {
    const auto snap = lpt::parse_proc_stat(kStat);
    EXPECT_EQ(snap.aggregate.user, 100u);
    EXPECT_EQ(snap.aggregate.nice, 10u);
    EXPECT_EQ(snap.aggregate.system, 50u);
    EXPECT_EQ(snap.aggregate.idle, 800u);
    EXPECT_EQ(snap.aggregate.iowait, 40u);
    EXPECT_EQ(snap.per_cpu.size(), 2u);
    EXPECT_EQ(snap.per_cpu[0].cpu_id, "cpu0");
    EXPECT_EQ(snap.context_switches, 123456u);
    EXPECT_EQ(snap.processes_created, 999u);
    EXPECT_EQ(snap.procs_running, 3u);
    EXPECT_EQ(snap.procs_blocked, 1u);
}

TEST(CpuUtilization, DeltaMath) {
    const auto a = lpt::parse_proc_stat(kStat).aggregate;
    const auto b = lpt::parse_proc_stat(kStat2).aggregate;
    const auto u = lpt::utilization_from_delta(a, b);
    EXPECT_GT(u.usage_ratio, 0.0);
    EXPECT_LE(u.usage_ratio, 1.0);
    EXPECT_NEAR(u.usage_ratio, 1.0 - (110.0 / 282.0), 1e-9);
}

TEST(MemoryParser, KeyFields) {
    const auto m = lpt::parse_meminfo(kMeminfo);
    EXPECT_EQ(m.mem_total_kb, 16384000u);
    EXPECT_EQ(m.mem_available_kb, 10240000u);
    EXPECT_EQ(m.anon_pages_kb, 3145728u);
    EXPECT_EQ(m.swap_total_kb, 2097152u);
}

TEST(VmstatParser, FaultsAndSwap) {
    const auto v = lpt::parse_vmstat(kVmstat);
    EXPECT_EQ(v.pgfault, 555000u);
    EXPECT_EQ(v.pgmajfault, 1200u);
    EXPECT_EQ(v.pswpin, 10u);
    EXPECT_EQ(v.nr_dirty, 12u);
}

TEST(DiskParser, SkipsPartitionsAndLoop) {
    const auto d = lpt::parse_diskstats(kDiskstats, true);
    ASSERT_EQ(d.devices.size(), 2u);
    EXPECT_EQ(d.devices[0].name, "sda");
    EXPECT_EQ(d.devices[0].reads_completed, 100u);
    EXPECT_EQ(d.devices[0].sectors_written, 16000u);
    EXPECT_EQ(d.devices[1].name, "nvme0n1");
}

TEST(ProcessParser, CommWithStatFields) {
    auto p = lpt::parse_proc_pid_stat(42, kPidStat);
    EXPECT_EQ(p.comm, "lpt-agent");
    EXPECT_EQ(p.state, 'S');
    EXPECT_EQ(p.minflt, 100u);
    EXPECT_EQ(p.majflt, 3u);
    EXPECT_EQ(p.utime, 25u);
    EXPECT_EQ(p.stime, 10u);
    EXPECT_EQ(p.num_threads, 4);
    EXPECT_EQ(p.rss_pages, 2048);
    lpt::apply_status_fields(p, kStatus);
    lpt::apply_io_fields(p, kIo);
    EXPECT_EQ(p.voluntary_ctxt_switches, 77u);
    EXPECT_EQ(p.write_bytes, 8192u);
}

TEST(Prometheus, EscapesLabelsAndRendersType) {
    EXPECT_EQ(lpt::escape_label_value("a\"b\\c"), "a\\\"b\\\\c");
    lpt::MetricsRegistry r;
    r.set_gauge("lpt_example", "help text", 1.5, {{"device", "sda"}});
    const std::string text = r.render();
    EXPECT_NE(text.find("# TYPE lpt_example gauge"), std::string::npos);
    EXPECT_NE(text.find("lpt_example{device=\"sda\"} 1.5"), std::string::npos);
}

TEST(Exporter, EmitsCoreMetricsFromTwoScrapes) {
    lpt::SystemSnapshot s1;
    s1.cpu = lpt::parse_proc_stat(kStat);
    s1.memory = lpt::parse_meminfo(kMeminfo);
    s1.vm = lpt::parse_vmstat(kVmstat);
    s1.disk = lpt::parse_diskstats(kDiskstats, true);
    auto proc = lpt::parse_proc_pid_stat(42, kPidStat);
    lpt::apply_status_fields(proc, kStatus);
    lpt::apply_io_fields(proc, kIo);
    s1.processes.processes.push_back(proc);

    lpt::SnapshotExporter ex(8);
    ex.update(s1);
    lpt::SystemSnapshot s2 = s1;
    s2.cpu = lpt::parse_proc_stat(kStat2);
    ex.update(s2);
    const std::string body = ex.render();
    EXPECT_NE(body.find("lpt_cpu_usage_ratio"), std::string::npos);
    EXPECT_NE(body.find("lpt_context_switches_total"), std::string::npos);
    EXPECT_NE(body.find("lpt_page_faults_total"), std::string::npos);
    EXPECT_NE(body.find("lpt_disk_reads_completed_total"), std::string::npos);
    EXPECT_NE(body.find("lpt_process_rss_bytes"), std::string::npos);
}

TEST(ProcFs, ReadsFixtureTree) {
    lpt::ProcFs fs(LPT_FIXTURE_DIR);
    const auto cpu = lpt::collect_cpu(fs);
    EXPECT_EQ(cpu.aggregate.user, 3357u);
    EXPECT_EQ(cpu.context_switches, 1111u);
    const auto mem = lpt::collect_memory(fs);
    EXPECT_EQ(mem.mem_total_kb, 2048000u);
    const auto procs = lpt::collect_processes(fs, 10);
    ASSERT_FALSE(procs.processes.empty());
    EXPECT_EQ(procs.processes[0].pid, 1);
    EXPECT_EQ(procs.processes[0].comm, "init");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
