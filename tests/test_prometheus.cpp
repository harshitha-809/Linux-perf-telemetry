#include "lpt/metrics_registry.hpp"

#include <gtest/gtest.h>

using namespace lpt;

TEST(Prometheus, RendersHelpTypeAndLabels) {
    MetricsRegistry r;
    r.set_gauge("lpt_cpu_usage_ratio", "CPU busy fraction", 0.25, {{"cpu", "all"}});
    r.set_counter("lpt_forks_total", "forks", 12);
    const std::string text = r.render_prometheus();
    EXPECT_NE(text.find("# HELP lpt_cpu_usage_ratio CPU busy fraction"), std::string::npos);
    EXPECT_NE(text.find("# TYPE lpt_cpu_usage_ratio gauge"), std::string::npos);
    EXPECT_NE(text.find("lpt_cpu_usage_ratio{cpu=\"all\"} 0.25"), std::string::npos);
    EXPECT_NE(text.find("# TYPE lpt_forks_total counter"), std::string::npos);
    EXPECT_NE(text.find("lpt_forks_total 12"), std::string::npos);
}

TEST(Prometheus, EscapesQuotesInLabels) {
    MetricsRegistry r;
    r.set_gauge("m", "h", 1.0, {{"comm", "a\"b"}});
    EXPECT_NE(r.render_prometheus().find("comm=\"a\\\"b\""), std::string::npos);
}
