#include "lpt/memory.hpp"

#include <gtest/gtest.h>

using namespace lpt;

TEST(Memory, ParsesMeminfoAndUsedRatio) {
    const char* text =
        "MemTotal:        8000000 kB\n"
        "MemFree:         2000000 kB\n"
        "MemAvailable:    4000000 kB\n"
        "Cached:          1500000 kB\n"
        "AnonPages:       3000000 kB\n";
    const auto m = parse_meminfo(text);
    EXPECT_EQ(m.mem_total_kb, 8000000u);
    EXPECT_EQ(m.mem_available_kb, 4000000u);
    EXPECT_EQ(m.anon_pages_kb, 3000000u);
    EXPECT_NEAR(memory_used_ratio(m), 0.5, 1e-9);
}
