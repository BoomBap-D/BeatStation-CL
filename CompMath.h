#pragma once
#include <cmath>

// Static gain curve with soft knee (all values in dB). Shared by the DSP and the transfer-curve display
// so the picture always matches what you hear.
namespace CompMath
{
    inline float staticCurve (float inDb, float threshDb, float ratio, float kneeDb)
    {
        const float over = inDb - threshDb;
        if (kneeDb > 0.01f && 2.f * std::abs (over) <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            return inDb + (1.f / ratio - 1.f) * t * t / (2.f * kneeDb);
        }
        return over <= 0.f ? inDb : threshDb + over / ratio;
    }
}
