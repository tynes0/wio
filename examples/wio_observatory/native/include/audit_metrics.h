#pragma once

#include <cstdint>

namespace observatory_native
{
    struct AuditMetrics
    {
        std::uint64_t files;
        std::uint64_t bytes;
        std::uint64_t warnings;
        double score;
    };

    void AddFile(AuditMetrics& metrics, std::uint64_t bytes, std::uint64_t warnings);
    bool MeetsThreshold(const AuditMetrics* metrics, double minimum);
    double Risk(const AuditMetrics& metrics);
}
