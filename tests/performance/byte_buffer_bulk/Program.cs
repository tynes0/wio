using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;

sealed class ByteBuffer
{
    public byte[] Data = new byte[2048];
    public int Count;
    public int Position;

    public void WriteBlock(byte[] values)
    {
        values.AsSpan().CopyTo(Data.AsSpan(Position));
        Position += values.Length;
        Count = Math.Max(Count, Position);
    }
}

static class Program
{
    private static ulong sink;

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static ulong Work(ByteBuffer buffer, byte[] input, long count)
    {
        byte[] source = (byte[])input.Clone();
        ulong checksum = 0;
        for (long index = 0; index < count; ++index)
        {
            buffer.Count = 0;
            buffer.Position = 0;
            buffer.WriteBlock(source);
            buffer.Position = 512;
            buffer.WriteBlock(source);
            buffer.Position = 0;
            buffer.WriteBlock(source);
            if (buffer.Count != 1536 || buffer.Position != 1024) throw new InvalidOperationException("Buffer state changed");
            checksum += (ulong)(buffer.Count + buffer.Position);
        }

        return checksum;
    }

    private static int Main(string[] args)
    {
        if (args.Length != 2 || args[0] != "0" || !long.TryParse(args[1], out long count)
            || count < 1 || count > 100000) return 1;
        byte[] source = new byte[1024];
        for (int index = 0; index < source.Length; ++index)
        {
            source[index] = (byte)((index * 17 + 3) & 255);
        }

        var buffer = new ByteBuffer();
        sink = Work(buffer, source, count);
        sink = Work(buffer, source, count);
        long start = Stopwatch.GetTimestamp();
        ulong checksum = Work(buffer, source, count);
        System.Threading.Volatile.Write(ref sink, checksum);
        long elapsed = (long)((Stopwatch.GetTimestamp() - start) * (1_000_000_000.0 / Stopwatch.Frequency));
        byte[] bytes = buffer.Data.AsSpan(0, buffer.Count).ToArray();
        ulong hash = 14695981039346656037;
        for (int index = 0; index < bytes.Length; ++index)
        {
            if (bytes[index] != (byte)((index * 17 + 3) & 255)) return 2;
            hash = unchecked((hash ^ bytes[index]) * 1099511628211);
        }

        Console.WriteLine("{\"schemaVersion\":1,\"checksum\":\"" + (checksum ^ hash)
            + "\",\"elapsedNanoseconds\":" + elapsed + ",\"operations\":" + count + "}");
        GC.KeepAlive(sink);
        return 0;
    }
}
