#include "telemetry_native.h"

#include <cmath>
#include <cstring>

namespace telemetry_native
{
    namespace
    {
        constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
        constexpr std::uint64_t kFnvPrime = 1099511628211ull;

        void hashBytes(std::uint64_t& hash, const void* data, const std::size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (std::size_t index = 0; index < size; ++index)
            {
                hash ^= bytes[index];
                hash *= kFnvPrime;
            }
        }
    }

    float Deviation(const Sample& sample)
    {
        return sample.value - sample.baseline;
    }

    float NormalizedScore(const Sample& sample)
    {
        const float scale = std::max(std::fabs(sample.baseline), 1.0f);
        return std::fabs(Deviation(sample)) / scale * 10.0f;
    }

    void Calibrate(Sample& sample, const float gain, const float offset)
    {
        sample.value = sample.value * gain + offset;
    }

    std::int32_t Classify(const Sample& sample, const float warningScore, const float criticalScore)
    {
        const float score = NormalizedScore(sample);
        if (score >= criticalScore)
            return 2;
        if (score >= warningScore)
            return 1;
        return 0;
    }

    std::uint64_t Fingerprint(const Sample& sample)
    {
        std::uint64_t hash = kFnvOffset;
        hashBytes(hash, &sample.sensorId, sizeof(sample.sensorId));
        hashBytes(hash, &sample.value, sizeof(sample.value));
        hashBytes(hash, &sample.baseline, sizeof(sample.baseline));
        hashBytes(hash, &sample.timestampMs, sizeof(sample.timestampMs));
        return hash;
    }
}
