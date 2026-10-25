#include "PostCommon.hlsli"
Texture2D g_scene : register(t0);
Texture2D g_bloom : register(t1);

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy * invSize;
    float3 scene = g_scene.Sample(g_linear, uv).rgb;
    float3 bloom = g_bloom.Sample(g_linear, uv).rgb;
    
    float3 hdr = scene + bloom * strength; // ① 선형 HDR에서 합성
    float3 ldr = ACESFilm(hdr * exposure); // ② 노출 → 톤매핑 (0~1)
    float3 gamma = pow(ldr, 1.0 / 2.2); // ③ 선형 → 감마 (모니터용)
    return float4(gamma, 1.0);
}