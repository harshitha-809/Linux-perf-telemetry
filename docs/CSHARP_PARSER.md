# C# Performance Parsing Demonstration

This repository contains a standalone C# mini-project (`csharp/FastProcParser`) to complement the primary C++ Linux telemetry implementation. It demonstrates fundamental C# proficiency, structured metric collection, and performance-aware parsing techniques.

## What this utility does
`FastProcParser` is a minimal .NET console application that reads `/proc/meminfo` and extracts key memory metrics (like `MemTotal` and `MemAvailable`). It prints these metrics to the console with clean error handling for missing files or unauthorized access.

## Why `/proc`?
On Linux, the `/proc` pseudofilesystem is the standard interface for querying kernel and process telemetry. The C++ project extensively queries `/proc`, and this C# utility mirrors that approach to demonstrate OS-level telemetry collection from managed .NET code.

## Performance-Aware Parsing
In C#, string manipulation can quickly introduce Garbage Collection (GC) overhead if not done carefully.

* **Why we avoid `String.Split`**: A common naive approach to parsing key-value pairs is `line.Split(':')`. This creates a new `string[]` array and multiple new `string` objects for every single line parsed, creating unnecessary memory allocations that must eventually be garbage collected.
* **Why we use `ReadOnlySpan<char>`**: By treating the line as a `ReadOnlySpan<char>`, we can use `IndexOf(':')` and `Slice()` to identify the boundaries of the key and the value directly over the existing memory. This significantly *reduces* temporary string and array allocations in the hot parsing path, keeping the application's memory footprint lower.

## Important Caveats

> **Notice:** This is a small educational/portfolio utility, not a production telemetry agent. It has been designed specifically to demonstrate conceptual C# patterns and memory-conscious coding.
>
> **Build constraints:** Because the `dotnet` SDK is not currently installed in the host environment, this project has **NOT** been compiled, executed, or locally benchmarked. No formal claims about exact allocation elimination or GC performance numbers are made, as they have not been empirically measured on this system.

## Build and Run Instructions
For machines where the .NET 8.0+ SDK is installed:

```bash
cd csharp/FastProcParser
dotnet build
dotnet run
```
