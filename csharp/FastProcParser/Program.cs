using System;
using System.IO;

namespace FastProcParser;

/// <summary>
/// A minimal, performance-conscious parser for Linux /proc/meminfo.
/// Demonstrates using ReadOnlySpan<char> to avoid string allocations in the hot parsing path.
/// </summary>
public static class Program
{
    private const string MemInfoPath = "/proc/meminfo";

    public static void Main(string[] args)
    {
        Console.WriteLine($"[FastProcParser] Attempting to read {MemInfoPath}...");

        try
        {
            if (!File.Exists(MemInfoPath))
            {
                Console.WriteLine($"Error: {MemInfoPath} does not exist. (Are you running on Windows without WSL?)");
                return;
            }

            ParseMemInfo(MemInfoPath);
        }
        catch (UnauthorizedAccessException)
        {
            Console.WriteLine($"Error: Permission denied reading {MemInfoPath}.");
        }
        catch (IOException ex)
        {
            Console.WriteLine($"Error: I/O exception occurred - {ex.Message}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Unexpected error: {ex.Message}");
        }
    }

    private static void ParseMemInfo(string filePath)
    {
        // ReadLines is used to prevent loading massive files entirely into memory at once.
        foreach (string line in File.ReadLines(filePath))
        {
            // We use ReadOnlySpan<char> to parse the line without allocating
            // a new string array from String.Split(':') or creating substrings.
            ReadOnlySpan<char> lineSpan = line.AsSpan();

            int colonIndex = lineSpan.IndexOf(':');
            if (colonIndex == -1) continue;

            // Extract the key (e.g., "MemTotal") and trim whitespace
            ReadOnlySpan<char> key = lineSpan.Slice(0, colonIndex).Trim();

            // Extract the value part (e.g., " 16393164 kB") and trim whitespace
            ReadOnlySpan<char> valuePart = lineSpan.Slice(colonIndex + 1).Trim();

            // Only parse specific interesting metrics to keep output readable
            if (key.SequenceEqual("MemTotal") ||
                key.SequenceEqual("MemFree") ||
                key.SequenceEqual("MemAvailable") ||
                key.SequenceEqual("SwapTotal"))
            {
                // Note: The value part often contains the unit, e.g., "1234 kB".
                // In a production agent we would further span-slice to extract just the integer,
                // but for this minimal demonstration, converting the sliced span back to a string for display is sufficient.
                Console.WriteLine($"{key.ToString()}: {valuePart.ToString()}");
            }
        }
    }
}
