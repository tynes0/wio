using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.CompilerServices;

static class Program
{
    private static ulong sink;

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Hash(byte[] bytes)
    {
        ulong checksum = 14695981039346656037;
        foreach (byte value in bytes)
        {
            checksum = unchecked((checksum ^ value) * 1099511628211);
        }

        return checksum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Work(FileStream file, long mode, long count)
    {
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            file.Seek(0, SeekOrigin.Begin);
            byte[] bytes = new byte[65536];
            int size;
            if (mode == 0)
            {
                file.ReadExactly(bytes);
                size = bytes.Length;
            }
            else
            {
                size = file.Read(bytes);
            }

            if (size != bytes.Length) Array.Resize(ref bytes, size);
            checksum = unchecked(checksum + Hash(bytes));
        }

        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || !long.TryParse(args[0], out long mode) || !long.TryParse(args[1], out long count)
            || mode < 0 || mode > 1 || count < 1 || count > 100000) return 1;
        byte[] payload = new byte[65536];
        for (int index = 0; index < payload.Length; ++index)
        {
            payload[index] = (byte)((index * 17 + 3) & 255);
        }

        File.WriteAllBytes("payload.bin", payload);
        using var file = new FileStream("payload.bin", FileMode.Open, FileAccess.Read, FileShare.Read, 1);
        sink = Work(file, mode, count);
        sink = Work(file, mode, count);
        long start = Stopwatch.GetTimestamp();
        ulong checksum = Work(file, mode, count);
        System.Threading.Volatile.Write(ref sink, checksum);
        long elapsed = (long)((Stopwatch.GetTimestamp() - start) * (1_000_000_000.0 / Stopwatch.Frequency));
        Console.WriteLine("{\"schemaVersion\":1,\"checksum\":\"" + checksum.ToString(CultureInfo.InvariantCulture)
            + "\",\"elapsedNanoseconds\":" + elapsed.ToString(CultureInfo.InvariantCulture)
            + ",\"operations\":" + count + "}");
        GC.KeepAlive(sink);
        return 0;
    }
}
