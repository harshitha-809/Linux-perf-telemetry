#include "lpt/collector.hpp"

#include <gtest/gtest.h>

#ifndef LPT_FIXTURE_DIR
#define LPT_FIXTURE_DIR "."
#endif

using namespace lpt;

TEST(Collector, ScrapesFixtureRootAndEmitsPrometheus) {
    CollectorConfig cfg;
    cfg.proc_root = LPT_FIXTURE_DIR;
    cfg.max_processes = 8;
    Collector c(cfg);
    c.scrape();
    c.scrape();
    const std::string text = c.prometheus_text();
    EXPECT_NE(text.find("lpt_cpu_usage_ratio"), std::string::npos);
    EXPECT_NE(text.find("lpt_memory_total_bytes"), std::string::npos);
    EXPECT_NE(text.find("lpt_page_faults_minor_per_second"), std::string::npos);
    EXPECT_NE(text.find("lpt_disk_reads_per_second"), std::string::npos);
    EXPECT_EQ(c.last_memory().mem_total_kb, 8000000u);
}
