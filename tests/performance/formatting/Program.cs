using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text;

static class Program
{
    private static ulong sink;

    private static string Number(double value)
    {
        // C++ defaultfloat has lowercase exponent and at least two exponent digits.
        return value.ToString("G6", CultureInfo.InvariantCulture).Replace('E', 'e');
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Hash(string value)
    {
        ulong checksum = 14695981039346656037;
        foreach (byte item in Encoding.UTF8.GetBytes(value))
        {
            checksum = unchecked((checksum ^ item) * 1099511628211);
        }

        return checksum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Work(long mode, long count)
    {
        ulong checksum = 0;
        if (mode == 0)
        {
            for (long index = 0; index < count; ++index)
            {
                checksum = unchecked(checksum + Hash(Number((double)(index % 64) * 0.125 - 4.0)));
            }

            return checksum;
        }

        if (mode == 1)
        {
            var builder = new StringBuilder(64);
            for (long index = 0; index < count; ++index)
            {
                builder.Append("reading=" + Number((double)(index % 64) * 0.125 - 4.0)
                    + ";sample=" + index.ToString(CultureInfo.InvariantCulture) + ";ok=true");
                builder.Append('\n');
            }

            string snapshot = builder.ToString();
            return Hash(snapshot);
        }

        double[] numbers = {-4.0, -0.125, 0.0, 0.125, 1.5, 7.875, 32.0, 12500000.0};
        for (long index = 0; index < count; ++index)
        {
            var value = new StringBuilder("[");
            for (int item = 0; item < numbers.Length; ++item)
            {
                if (item > 0) value.Append(", ");
                value.Append(Number(numbers[item]));
            }

            value.Append(']');
            checksum = unchecked(checksum + Hash(value.ToString()));
        }

        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || !long.TryParse(args[0], out long mode) || !long.TryParse(args[1], out long count)
            || mode < 0 || mode > 2 || count < 1 || count > 100000) return 1;
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
