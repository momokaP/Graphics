#include "PostCommon.hlsli"

Texture2D g_scene : register(t0);
Texture2D g_bloom : register(t1);

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy * g_post.invSize;
    float3 scene = g_scene.Sample(g_linearClamp, uv).rgb;
    float3 bloom = g_bloom.Sample(g_linearClamp, uv).rgb;

    float3 hdr = scene + bloom * g_post.strength;
    float3 ldr = ACESFilm(hdr * g_post.exposure);
    float3 gamma = pow(ldr, 1.0 / 2.2);
    return float4(gamma, 1.0);
}