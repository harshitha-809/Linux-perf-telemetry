#include "lpt/disk.hpp"

#include <gtest/gtest.h>

using namespace lpt;

TEST(Disk, SkipsPartitionsAndComputesThroughput) {
    const char* text =
        "   8       0 sda 100 10 8000 50 40 5 3200 80 0 120 130\n"
        "   8       1 sda1 90 8 7000 40 30 4 3000 70 0 100 110\n"
        "   7       0 loop0 1 0 8 0 0 0 0 0 0 0 0\n";
    const auto disks = parse_diskstats(text);
    ASSERT_EQ(disks.size(), 1u);
    EXPECT_EQ(disks[0].name, "sda");
    EXPECT_EQ(disks[0].reads_completed, 100u);
    EXPECT_EQ(disks[0].sectors_read, 8000u);

    auto later = disks;
    later[0].reads_completed = 200;
    later[0].sectors_read = 16000;
    later[0].writes_completed = 80;
    later[0].sectors_written = 6400;
    later[0].ms_io = 220;
    const auto rates = compute_disk_rates(disks, later, 1.0);
    ASSERT_EQ(rates.size(), 1u);
    EXPECT_NEAR(rates[0].reads_per_sec, 100.0, 1e-9);
    EXPECT_NEAR(rates[0].read_bytes_per_sec, 8000.0 * 512.0, 1e-6);
    EXPECT_NEAR(rates[0].writes_per_sec, 40.0, 1e-9);
    EXPECT_NEAR(rates[0].util_ratio, 0.1, 1e-9);
}
