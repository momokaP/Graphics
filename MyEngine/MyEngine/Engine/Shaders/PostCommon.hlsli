#ifndef POST_COMMON_HLSLI
#define POST_COMMON_HLSLI

#include "Bindings.hlsli"        // g_post(b3), g_linearClamp(s1)

float3 ACESFilm(float3 x)        // 근사 ACES 톤매핑 (Narkowicz)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

#endif