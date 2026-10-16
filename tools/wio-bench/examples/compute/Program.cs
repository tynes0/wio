using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;

static class Program
{
    private static ulong sink;

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Compute(long count)
    {
        ulong state = 12345;
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            state = state * 48271 % 2147483647;
            checksum += state;
        }
        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 1 || !long.TryParse(args[0], out long count) || count < 1 || count > 1000000) return 1;
        // Process warmups cannot warm another process's JIT. Warm the actual
        // method in THIS process, matching the native/Wio workload warmups.
        sink = Compute(count);
        sink = Compute(count);
        long allocated = GC.GetAllocatedBytesForCurrentThread();
        long start = Stopwatch.GetTimestamp();
        ulong checksum = Compute(count);
        System.Threading.Volatile.Write(ref sink, checksum);
        long end = Stopwatch.GetTimestamp();
        long bytes = GC.GetAllocatedBytesForCurrentThread() - allocated;
        long elapsed = (long)((end - start) * (1_000_000_000.0 / Stopwatch.Frequency));
        Console.WriteLine("{\"schemaVersion\":1,\"checksum\":\"" + checksum.ToString(CultureInfo.InvariantCulture)
            + "\",\"elapsedNanoseconds\":" + elapsed.ToString(CultureInfo.InvariantCulture)
            + ",\"operations\":" + count + ",\"managedAllocatedBytesCurrentThread\":" + bytes + "}");
        GC.KeepAlive(sink);
        return 0;
    }
}
