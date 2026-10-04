using System.Diagnostics;
using System.Runtime.CompilerServices;

internal readonly record struct Totals(ulong Iterations, ulong Weighted, ulong Interior);

internal static class Program
{
    [MethodImpl(MethodImplOptions.NoInlining | MethodImplOptions.AggressiveOptimization)]
    private static Totals Compute(int width, int height, int maxIterations)
    {
        ulong iterations = 0;
        ulong weighted = 0;
        ulong interior = 0;
        for (int py = 0; py < height; ++py)
        {
            for (int px = 0; px < width; ++px)
            {
                double x0 = ((double)px / width) * 3.5 - 2.5;
                double y0 = ((double)py / height) * 3.0 - 1.5;
                double x = 0.0;
                double y = 0.0;
                int iteration = 0;
                while (x * x + y * y <= 4.0 && iteration < maxIterations)
                {
                    double xtemp = x * x - y * y + x0;
                    y = 2.0 * x * y + y0;
                    x = xtemp;
                    ++iteration;
                }
                iterations += (ulong)iteration;
                weighted += (ulong)iteration * ((ulong)py * (ulong)width + (ulong)px + 1);
                if (iteration == maxIterations)
                    ++interior;
            }
        }
        return new Totals(iterations, weighted, interior);
    }

    public static int Main()
    {
        // JIT the same method before timing, using a distinct small input.
        _ = Compute(32, 32, 100);
        long start = Stopwatch.GetTimestamp();
        Totals totals = Compute(800, 800, 100);
        long end = Stopwatch.GetTimestamp();
        double seconds = (double)(end - start) / Stopwatch.Frequency;
        Console.WriteLine(FormattableString.Invariant(
            $"seconds={seconds:F9} iterations={totals.Iterations} weighted={totals.Weighted} interior={totals.Interior}"));
        return totals == new Totals(11_613_930, 3_721_766_287_256, 94_440) ? 0 : 1;
    }
}
