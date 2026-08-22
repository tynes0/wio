#pragma once

#include <cstdint>

namespace telemetry_native
{
    struct Sample
    {
        std::int32_t sensorId;
        float value;
        float baseline;
        std::uint64_t timestampMs;
    };

    float Deviation(const Sample& sample);
    float NormalizedScore(const Sample& sample);
    void Calibrate(Sample& sample, float gain, float offset);
    std::int32_t Classify(const Sample& sample, float warningScore, float criticalScore);
    std::uint64_t Fingerprint(const Sample& sample);
}
