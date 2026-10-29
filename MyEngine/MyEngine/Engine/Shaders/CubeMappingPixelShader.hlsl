#include "SceneCommon.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    float3 hdr = g_envMap.Sample(g_linearClamp, input.posWorld).rgb;
    return float4(hdr, 1.0);
}