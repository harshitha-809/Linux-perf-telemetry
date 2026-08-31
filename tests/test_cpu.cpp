#include "lpt/cpu.hpp"

#include <gtest/gtest.h>

using namespace lpt;

TEST(Cpu, ParsesProcStat) {
    const char* text =
        "cpu  1000 0 500 8500 0 0 0 0 0 0\n"
        "cpu0 500 0 250 4250 0 0 0 0 0 0\n"
        "ctxt 12000\n"
        "processes 80\n"
        "procs_running 2\n"
        "procs_blocked 1\n"
        "intr 4000\n";
    const auto s = parse_proc_stat(text);
    EXPECT_EQ(s.aggregate.user, 1000u);
    EXPECT_EQ(s.aggregate.system, 500u);
    EXPECT_EQ(s.aggregate.idle, 8500u);
    EXPECT_EQ(s.per_cpu.size(), 1u);
    EXPECT_EQ(s.context_switches, 12000u);
    EXPECT_EQ(s.procs_running, 2u);
    EXPECT_EQ(s.interrupts, 4000u);
}

TEST(Cpu, ComputesUtilizationAndCswitchRate) {
    CpuSnapshot a;
    a.aggregate.user = 100;
    a.aggregate.system = 50;
    a.aggregate.idle = 850;
    a.context_switches = 1000;
    a.per_cpu.push_back(a.aggregate);

    CpuSnapshot b = a;
    b.aggregate.user = 200;
    b.aggregate.system = 100;
    b.aggregate.idle = 1700;
    b.context_switches = 1500;
    b.per_cpu[0] = b.aggregate;

    const auto r = compute_cpu_rates(a, b, 1.0);
    // busy delta = 150, total delta = 1000 -> 0.15
    EXPECT_NEAR(r.usage_ratio, 0.15, 1e-9);
    EXPECT_NEAR(r.user_ratio, 0.10, 1e-9);
    EXPECT_NEAR(r.system_ratio, 0.05, 1e-9);
    EXPECT_NEAR(r.context_switches_per_sec, 500.0, 1e-9);
    ASSERT_EQ(r.per_cpu_usage.size(), 1u);
    EXPECT_NEAR(r.per_cpu_usage[0], 0.15, 1e-9);
}
