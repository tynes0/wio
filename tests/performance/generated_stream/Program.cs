using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text;

static class Program
{
    private static ulong sink;
    private static readonly string Small = Payload(192);
    private static readonly string Large = Payload(8192);

    private static string Payload(int size)
    {
        var chars = new char[size];
        for (int index = 0; index < size; ++index)
        {
            chars[index] = index % 31 == 0 ? '\0' : (char)('a' + index % 26);
        }

        return new string(chars);
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
        for (long index = 0; index < count; ++index)
        {
            var stream = new StringBuilder();
            if (mode == 1) stream.Append("prefix\0");
            stream.Append("payload=");
            stream.Append(mode == 0 ? Small : Large);
            stream.Append(";index=");
            stream.Append(index.ToString(CultureInfo.InvariantCulture));
            stream.Append(";ok=");
            stream.Append(index % 2 == 0 ? "true" : "false");
            if (mode == 1) stream.Append("suffix");
            checksum = unchecked(checksum + Hash(stream.ToString()));
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
