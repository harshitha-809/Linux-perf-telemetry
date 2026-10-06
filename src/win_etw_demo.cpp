#include <windows.h>
#include <TraceLoggingProvider.h>
#include <iostream>
#include <thread>
#include <chrono>

// Define the custom provider for this project
// Name: "LinuxPerfTelemetry-WinEtwDemo"
TRACELOGGING_DECLARE_PROVIDER(g_hMyProvider);
TRACELOGGING_DEFINE_PROVIDER(
    g_hMyProvider,
    "LinuxPerfTelemetry-WinEtwDemo",
    (0xb4518c23, 0x1803, 0x4f12, 0x93, 0x63, 0x3d, 0x1b, 0xc2, 0x1f, 0x86, 0x55));

void run_simulated_batch(int batchId) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Emit a Start event
    TraceLoggingWrite(
        g_hMyProvider,
        "BatchStart",
        TraceLoggingInt32(batchId, "BatchID"),
        TraceLoggingString("Starting lightweight simulated work", "Message")
    );

    // Simulate work (very lightweight to prevent thermal stress)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Emit a Stop event
    TraceLoggingWrite(
        g_hMyProvider,
        "BatchStop",
        TraceLoggingInt32(batchId, "BatchID"),
        TraceLoggingInt64(duration_ms, "DurationMs"),
        TraceLoggingString("Finished lightweight simulated work", "Message")
    );
}

int main() {
    std::cout << "Starting Windows ETW TraceLogging Demo...\n";

    // Register the provider
    HRESULT hr = TraceLoggingRegister(g_hMyProvider);
    if (FAILED(hr)) {
        std::cerr << "Failed to register TraceLogging provider. HR: 0x"
                  << std::hex << hr << "\n";
        return 1;
    }

    std::cout << "Provider registered successfully.\n";
    std::cout << "Running 10 simulated batches...\n";

    // Run a fixed, small number of iterations to keep it safe
    for (int i = 1; i <= 10; ++i) {
        run_simulated_batch(i);
    }

    std::cout << "Batches completed.\n";

    // Unregister the provider
    TraceLoggingUnregister(g_hMyProvider);
    std::cout << "Provider unregistered. Demo complete.\n";

    return 0;
}
