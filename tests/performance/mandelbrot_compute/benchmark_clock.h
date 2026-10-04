#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace mandelbrot_audit
{
    inline std::int64_t Now()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    inline std::int32_t Report(const std::int64_t start, const std::int64_t end, const std::uint64_t iterations,
                               const std::uint64_t weighted, const std::uint64_t interior)
    {
        // Formatting happens only after the timed kernel has finished.
        std::cout << std::fixed << std::setprecision(9)
                  << "seconds=" << static_cast<double>(end - start) / 1'000'000'000.0 << " iterations=" << iterations
                  << " weighted=" << weighted << " interior=" << interior << '\n';
        return iterations == 11'613'930 && weighted == 3'721'766'287'256 && interior == 94'440 ? 0 : 1;
    }
} // namespace mandelbrot_audit
