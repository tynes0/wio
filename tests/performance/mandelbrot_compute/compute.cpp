#include "benchmark_clock.h"

struct Totals
{
    std::uint64_t iterations = 0;
    std::uint64_t weighted = 0;
    std::uint64_t interior = 0;
};

Totals Compute(const int width, const int height, const int maxIterations)
{
    Totals totals;
    for (int py = 0; py < height; ++py)
    {
        for (int px = 0; px < width; ++px)
        {
            const double x0 = (static_cast<double>(px) / static_cast<double>(width)) * 3.5 - 2.5;
            const double y0 = (static_cast<double>(py) / static_cast<double>(height)) * 3.0 - 1.5;
            double x = 0.0;
            double y = 0.0;
            int iteration = 0;
            while (x * x + y * y <= 4.0 && iteration < maxIterations)
            {
                const double xtemp = x * x - y * y + x0;
                y = 2.0 * x * y + y0;
                x = xtemp;
                ++iteration;
            }
            totals.iterations += static_cast<std::uint64_t>(iteration);
            totals.weighted +=
                static_cast<std::uint64_t>(iteration) * (static_cast<std::uint64_t>(py) * width + px + 1);
            if (iteration == maxIterations)
                ++totals.interior;
        }
    }
    return totals;
}

int main()
{
    const auto start = mandelbrot_audit::Now();
    const auto totals = Compute(800, 800, 100);
    const auto end = mandelbrot_audit::Now();
    return mandelbrot_audit::Report(start, end, totals.iterations, totals.weighted, totals.interior);
}
