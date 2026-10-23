#include "PostCommon.hlsli"
Texture2D g_scene : register(t0);
Texture2D g_bloom : register(t1);

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy * invSize;
    float3 scene = g_scene.Sample(g_linear, uv).rgb;
    float3 bloom = g_bloom.Sample(g_linear, uv).rgb; // 절반 해상도 → bilinear 업샘플
    return float4(scene + bloom * strength, 1.0); // UNORM 백버퍼에서 1 이상은 잘림
}