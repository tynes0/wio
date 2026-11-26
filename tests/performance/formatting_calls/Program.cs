using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text;

static class Program
{
    private static ulong sink;

    private class Counter
    {
        private ulong count;

        public override string ToString()
        {
            return "item:" + (++count).ToString(CultureInfo.InvariantCulture);
        }
    }

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
        var counter = new Counter();
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            string value;
            if (mode == 0)
            {
                string first = counter.ToString();
                value = "<" + first + ">";
            }
            else
            {
                string number = index.ToString(CultureInfo.InvariantCulture);
                string first = counter.ToString();
                string second = counter.ToString();
                value = number + ":" + first + "|" + second + ":true";
            }
            checksum = unchecked(checksum + Hash(value));
        }
        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || !long.TryParse(args[0], out long mode) || !long.TryParse(args[1], out long count)
            || mode < 0 || mode > 1 || count < 1 || count > 100000) return 1;
        sink = Work(mode, count);
        sink = Work(mode, count);
        long allocated = GC.GetAllocatedBytesForCurrentThread();
        long start = Stopwatch.GetTimestamp();
        ulong checksum = Work(mode, count);
        System.Threading.Volatile.Write(ref sink, checksum);
        long elapsed = (long)((Stopwatch.GetTimestamp() - start) * (1_000_000_000.0 / Stopwatch.Frequency));
        long bytes = GC.GetAllocatedBytesForCurrentThread() - allocated;
        Console.WriteLine("{\"schemaVersion\":1,\"checksum\":\"" + checksum.ToString(CultureInfo.InvariantCulture)
            + "\",\"elapsedNanoseconds\":" + elapsed.ToString(CultureInfo.InvariantCulture)
            + ",\"operations\":" + count + ",\"managedAllocatedBytesCurrentThread\":" + bytes + "}");
        GC.KeepAlive(sink);
        return 0;
    }
}
