using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text;

static class Program
{
    private static ulong sink;

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Hash(string value)
    {
        ulong checksum = 14695981039346656037;
        foreach (byte item in Encoding.UTF8.GetBytes(value))
            checksum = unchecked((checksum ^ item) * 1099511628211);
        return checksum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Work(long mode, long count)
    {
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            string row = mode == 0
                ? string.Format(CultureInfo.InvariantCulture, "{0:D8}/{1:X8}", index, index)
                : string.Format(CultureInfo.InvariantCulture, "{0:F3}/{1,-8}", index / 8.0, "hi");
            checksum = unchecked(checksum + Hash(row));
        }

        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || !long.TryParse(args[0], out long mode) || !long.TryParse(args[1], out long count)
            || mode < 0 || mode > 1 || count < 1 || count > 100000) return 1;
        sink = Work(mode, count);
        sink = Work(mode, count);
        long start = Stopwatch.GetTimestamp();
        ulong checksum = Work(mode, count);
        System.Threading.Volatile.Write(ref sink, checksum);
        long elapsed = (long)((Stopwatch.GetTimestamp() - start) * (1_000_000_000.0 / Stopwatch.Frequency));
        Console.WriteLine("{\"schemaVersion\":1,\"checksum\":\"" + checksum.ToString(CultureInfo.InvariantCulture)
            + "\",\"elapsedNanoseconds\":" + elapsed.ToString(CultureInfo.InvariantCulture)
            + ",\"operations\":" + count + "}");
        GC.KeepAlive(sink);
        return 0;
    }
}
