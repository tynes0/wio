#include "audit_metrics.h"

#include <algorithm>

namespace observatory_native
{
    void AddFile(AuditMetrics& metrics, std::uint64_t bytes, std::uint64_t warnings)
    {
        metrics.files += 1;
        metrics.bytes += bytes;
        metrics.warnings += warnings;
        metrics.score = std::max(0.0, 100.0 - static_cast<double>(metrics.warnings) * 12.0);
    }

    bool MeetsThreshold(const AuditMetrics* metrics, double minimum)
    {
        return metrics != nullptr && metrics->score >= minimum;
    }

    double Risk(const AuditMetrics& metrics)
    {
        return 100.0 - metrics.score;
    }
}
