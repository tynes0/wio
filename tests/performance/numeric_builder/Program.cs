using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text;

static class Program
{
    private static ulong sink;

    private static void Append(StringBuilder target, double value)
    {
        Span<char> buffer = stackalloc char[128];
        if (!value.TryFormat(buffer, out int written, "G6", CultureInfo.InvariantCulture))
            throw new InvalidOperationException("reference numeric buffer exhausted");
        for (int index = 0; index < written; ++index)
        {
            if (buffer[index] == 'E') buffer[index] = 'e';
        }

        target.Append(buffer[..written]);
    }

    private static void Append<T>(StringBuilder target, T value) where T : ISpanFormattable
    {
        Span<char> buffer = stackalloc char[128];
        if (!value.TryFormat(buffer, out int written, default, CultureInfo.InvariantCulture))
            throw new InvalidOperationException("reference numeric buffer exhausted");
        target.Append(buffer[..written]);
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
        var builder = new StringBuilder(mode == 2 ? (int)count * 16 : 64);
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            if (mode == 0)
            {
                builder.Append("id=");
                Append(builder, (ulong)index);
                builder.Append(",value=");
                Append(builder, (double)(index % 64) * 0.125 - 4.0);
                builder.Append(",ok=");
                builder.Append(index % 2 == 0 ? "true" : "false");
                builder.Append('\n');
            }
            else if (mode == 1)
            {
                builder.Clear();
                Append(builder, -9223372036854775807L + index);
                builder.Append(':');
                Append(builder, 18446744073709551615UL - (ulong)index);
                builder.Append(':');
                Append(builder, (sbyte)(index % 127));
                checksum = unchecked(checksum + Hash(builder.ToString()));
            }
            else
            {
                Append(builder, (double)((float)(index % 64) * 0.125f - 4.0f));
                builder.Append(',');
            }
        }

        if (mode == 1) return checksum;
        string snapshot = builder.ToString();
        return Hash(snapshot);
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
