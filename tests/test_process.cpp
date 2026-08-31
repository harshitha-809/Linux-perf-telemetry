#include "lpt/process.hpp"

#include <gtest/gtest.h>

using namespace lpt;

TEST(Process, ParsesStatWithSpacesInComm) {
    const std::string stat =
        "42 (my worker) R 1 42 42 0 -1 4194304 100 0 2 0 30 10 0 0 20 0 2 0 123 "
        "10485760 512 0 0 0 0 0 0 0 0 0 0 0 0 17 0 0 0 0 0 0\n";
    const auto s = parse_proc_pid_stat(42, stat);
    EXPECT_EQ(s.pid, 42);
    EXPECT_EQ(s.comm, "my worker");
    EXPECT_EQ(s.state, 'R');
    EXPECT_EQ(s.minflt, 100u);
    EXPECT_EQ(s.majflt, 2u);
    EXPECT_EQ(s.utime, 30u);
    EXPECT_EQ(s.stime, 10u);
    EXPECT_EQ(s.vsize, 10485760u);
    EXPECT_EQ(s.rss_pages, 512);
}

TEST(Process, EnrichesStatusAndIoAndRates) {
    ProcessSample a;
    a.pid = 1;
    a.utime = 10;
    a.stime = 5;
    a.minflt = 100;
    a.nvcsw = 10;
    a.read_bytes = 0;
    a.rss_pages = 10;

    ProcessSample b = a;
    b.utime = 30;
    b.stime = 15;
    b.minflt = 300;
    b.nvcsw = 30;
    b.read_bytes = 4096;
    b.rss_pages = 20;
    b.comm = "x";

    enrich_from_status(b, "Threads:\t4\nvoluntary_ctxt_switches:\t30\nnonvoluntary_ctxt_switches:\t1\n");
    EXPECT_EQ(b.threads, 4);
    enrich_from_io(b, "read_bytes: 4096\nwrite_bytes: 0\n");

    const auto rates = compute_process_rates({a}, {b}, 1.0, 100, 4096);
    ASSERT_EQ(rates.size(), 1u);
    EXPECT_NEAR(rates[0].cpu_ratio, 0.30, 1e-9);  // 30 ticks / 100 Hz / 1s
    EXPECT_NEAR(rates[0].minor_faults_per_sec, 200.0, 1e-9);
    EXPECT_EQ(rates[0].rss_bytes, 20u * 4096u);
}
