#pragma once

namespace hybrid_arena
{
    int ScaleDamage(int baseDamage, int difficulty100, int phase, int rage);
    int ComputeShieldGain(int phase, float deltaTime);
    int ComputeRageGain(float deltaTime, int pulseCount);
    int ComputePulseShield(float deltaTime, int pulseCount);
}
