#include "PostCommon.hlsli"

Texture2D g_src : register(t0);

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy * invSize;
    float3 c = g_src.Sample(g_linear, uv).rgb;
    float lum = dot(c, float3(0.2126, 0.7152, 0.0722));
    float k = max(lum - threshold, 0.0) / max(lum, 1e-4); // threshold 넘는 만큼만 부드럽게 남김
    return float4(c * k, 1.0);
}