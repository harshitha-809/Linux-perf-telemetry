# Windows Performance Analysis & ETW

While this project is primarily designed for Linux `perf` and `/proc` telemetry, this repository also includes a minimal demonstration of **Event Tracing for Windows (ETW)** to illustrate cross-platform performance engineering concepts.

## What is ETW?
Event Tracing for Windows (ETW) is a high-performance, low-overhead tracing facility provided by the Windows operating system. It is roughly analogous to Linux `perf` and `ftrace`. It allows kernel and user-mode applications to emit structured events that can be recorded system-wide.

## What is TraceLogging?
This project uses **TraceLogging**, a modern API built on top of ETW. Unlike classic manifest-based ETW (which requires compiling XML manifests), TraceLogging allows developers to emit self-describing, structured events directly from C++ code using simple macros.

## The Demonstration
The source code for the demonstration is located at `src/win_etw_demo.cpp`.

This demo creates a custom provider named `LinuxPerfTelemetry-WinEtwDemo`. It simulates a small batch-processing workload and emits structured `BatchStart` and `BatchStop` events, annotating them with a `BatchID` and `DurationMs`.

> **Note on Safety:** This demo is intentionally lightweight. It runs only 10 short iterations using `Sleep()` to minimize CPU and thermal load. It is safe to run on constrained laptops and VMs.

## How to Capture a Trace (WPR)
To capture a trace of this demo, you use **Windows Performance Recorder (WPR)**, which is built into modern Windows versions.

1. Open an Administrator command prompt.
2. Start recording system activity and user events:
   ```cmd
   wpr -start GeneralProfile
   ```
3. Run the compiled demo:
   ```cmd
   .\build\lpt-win-etw.exe
   ```
4. Stop the recording and save it to an `.etl` (Event Trace Log) file:
   ```cmd
   wpr -stop MyTrace.etl
   ```

## How to Analyze the Trace (WPA)
To inspect the trace, you use **Windows Performance Analyzer (WPA)**, available in the Windows ADK or the Microsoft Store.

1. Open `MyTrace.etl` in WPA.
2. Under the **System Activity** graph, look for **Generic Events**.
3. Filter by our Provider Name: `LinuxPerfTelemetry-WinEtwDemo`.
4. You will see the exact `BatchStart` and `BatchStop` events with their custom payloads (`BatchID`, `DurationMs`).

WPA can also be used to correlate these custom user-mode events with precise kernel metrics (CPU usage, context switches, disk I/O, and thread wait times) to identify exact bottlenecks in a production environment.

## Conclusion
This minimal integration demonstrates the ability to instrument Windows C++ code using modern ETW APIs and leverage the WPR/WPA ecosystem. Extending this to a fleet-wide production system would require further deployment strategies (like using In-Proc ETW listeners, Event Tracing Sessions, or telemetry agents), but the fundamental instrumentation remains the same.
