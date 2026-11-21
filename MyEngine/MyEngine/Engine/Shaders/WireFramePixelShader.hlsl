#include "SceneCommon.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    return float4(g_debug.tint, 1.0);
}