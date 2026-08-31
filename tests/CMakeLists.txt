add_executable(lpt_tests
    test_cpu.cpp
    test_memory.cpp
    test_vmstat.cpp
    test_disk.cpp
    test_process.cpp
    test_prometheus.cpp
    test_collector.cpp
)
target_link_libraries(lpt_tests PRIVATE lpt GTest::gtest_main)
target_compile_definitions(lpt_tests PRIVATE
    LPT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

include(GoogleTest)
gtest_discover_tests(lpt_tests)
