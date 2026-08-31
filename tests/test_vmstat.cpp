#include "lpt/vmstat.hpp"

#include <gtest/gtest.h>

using namespace lpt;

TEST(Vmstat, ParsesAndRatesSplitMinorMajor) {
    const auto a = parse_vmstat("pgfault 1000\npgmajfault 10\npgpgin 5\n");
    const auto b = parse_vmstat("pgfault 1300\npgmajfault 20\npgpgin 15\n");
    EXPECT_EQ(a.pgfault, 1000u);
    const auto r = compute_vmstat_rates(a, b, 2.0);
    EXPECT_NEAR(r.major_faults_per_sec, 5.0, 1e-9);
    EXPECT_NEAR(r.minor_faults_per_sec, 145.0, 1e-9);  // (300-10)/2
    EXPECT_NEAR(r.pages_in_per_sec, 5.0, 1e-9);
}
