using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;

static class Program
{
    private static ulong sink;

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Work(FileStream file, byte[] buffer, long mode, long count)
    {
        ulong written = 0;
        for (long index = 0; index < count; ++index)
        {
            file.Seek(0, SeekOrigin.Begin);
            if (mode == 0)
            {
                byte[] copy = buffer.AsSpan(7, 65519).ToArray();
                file.Write(copy);
            }
            else
            {
                file.Write(buffer.AsSpan(7, 65519));
            }

            written += 65519;
        }

        return written;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || !long.TryParse(args[0], out long mode) || !long.TryParse(args[1], out long count)
            || mode < 0 || mode > 1 || count < 1 || count > 100000) return 1;
        byte[] buffer = new byte[65536];
        for (int index = 0; index < buffer.Length; ++index)
        {
            buffer[index] = (byte)((index * 17 + 3) & 255);
        }

        using var output = new FileStream("output.bin", FileMode.Create, FileAccess.Write, FileShare.None, 1);
        sink = Work(output, buffer, mode, count);
        sink = Work(output, buffer, mode, count);
        long start = Stopwatch.GetTimestamp();
        ulong written = Work(output, buffer, mode, count);
        System.Threading.Volatile.Write(ref sink, written);
        long elapsed = (long)((Stopwatch.GetTimestamp() - start) * (1_000_000_000.0 / Stopwatch.Frequency));
        output.Close();
        byte[] bytes = File.ReadAllBytes("output.bin");
        if (bytes.Length != 65519) return 2;
        ulong checksum = 14695981039346656037;
        for (int index = 0; index < bytes.Length; ++index)
        {
            if (bytes[index] != buffer[index + 7]) return 3;
            checksum = unchecked((checksum ^ bytes[index]) * 1099511628211);
        }

        checksum ^= written;
        Console.WriteLine("{\"schemaVersion\":1,\"checksum\":\"" + checksum
            + "\",\"elapsedNanoseconds\":" + elapsed + ",\"operations\":" + count + "}");
        GC.KeepAlive(sink);
        return 0;
    }
}
