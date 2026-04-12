#include "hybrid_arena_native.h"

namespace hybrid_arena
{
    int ScaleDamage(int baseDamage, int difficulty100, int phase, int rage)
    {
        const int phaseBonus = phase > 1 ? (phase - 1) * 6 : 0;
        const int rageBonus = rage / 12;
        return (baseDamage * difficulty100) / 100 + phaseBonus + rageBonus;
    }

    int ComputeShieldGain(int phase, float deltaTime)
    {
        return phase + static_cast<int>(deltaTime * 2.0f);
    }

    int ComputeRageGain(float deltaTime, int pulseCount)
    {
        return static_cast<int>(deltaTime * 4.0f) + (pulseCount % 2);
    }

    int ComputePulseShield(float deltaTime, int pulseCount)
    {
        return pulseCount % 2;
    }
}
