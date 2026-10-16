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
        ulong result = 14695981039346656037;
        foreach (byte valueByte in Encoding.UTF8.GetBytes(value))
        {
            result = unchecked((result ^ valueByte) * 1099511628211);
        }

        return result;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Work(long mode, long count)
    {
        const string a = "copy\0payload:0123456789abcdef0123456789abcdef";
        const string b = "second:0123456789abcdef0123456789abcdef";
        const string c = "third:0123456789abcdef0123456789abcdef";
        if (mode < 2)
        {
            string source = mode == 0 ? a : "A🌍é日本語:payload:0123456789abcdef";
            string output = "";
            for (long index = 0; index < count; ++index)
            {
                output = output + source;
            }

            return Hash(output);
        }

        string ta = mode == 2 ? a : "A🌍é日本語:payload:0123456789abcdef";
        string tb = mode == 2 ? b : "B🌙é日本語:payload:0123456789abcdef";
        string tc = mode == 2 ? c : "C⭐é日本語:payload:0123456789abcdef";
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            string value = ta + tb + tc + ta + tb + tc;
            checksum ^= Hash(value);
        }

        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || !long.TryParse(args[0], out long mode) || !long.TryParse(args[1], out long count)
            || mode < 0 || mode > 3 || count < 1 || count > 100000) return 1;
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
