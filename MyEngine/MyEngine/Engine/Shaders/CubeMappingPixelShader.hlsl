#include "SceneCommon.hlsli"

TextureCube g_textureCube0 : register(t0);
SamplerState g_sampler : register(s1);


float4 main(PSInput input) : SV_TARGET
{
    float3 hdr = g_textureCube0.Sample(g_sampler, input.posWorld).rgb;
    return float4(hdr, 1.0);
}