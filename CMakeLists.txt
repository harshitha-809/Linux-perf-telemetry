cmake_minimum_required(VERSION 3.16)
project(linux-perf-telemetry VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LPT_BUILD_TESTS "Build unit tests" ON)
option(LPT_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

function(lpt_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(LPT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(lpt_core
    src/proc_fs.cpp
    src/collectors.cpp
    src/prometheus.cpp
    src/exporter.cpp
    src/http_server.cpp
    src/workload.cpp
)
target_include_directories(lpt_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
lpt_warnings(lpt_core)

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(lpt_core PUBLIC Threads::Threads)
endif()

add_executable(lpt-agent src/agent_main.cpp)
target_link_libraries(lpt-agent PRIVATE lpt_core)
lpt_warnings(lpt-agent)

add_executable(lpt-workload src/workload_main.cpp)
target_link_libraries(lpt-workload PRIVATE lpt_core)
lpt_warnings(lpt-workload)

if(LPT_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(lpt_tests tests/test_collectors.cpp)
    target_link_libraries(lpt_tests PRIVATE lpt_core GTest::gtest)
    target_compile_definitions(lpt_tests PRIVATE
        LPT_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/proc")
    lpt_warnings(lpt_tests)
    include(GoogleTest)
    gtest_discover_tests(lpt_tests)
endif()

install(TARGETS lpt-agent lpt-workload RUNTIME DESTINATION bin)
