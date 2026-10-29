#include "PostCommon.hlsli"

Texture2D g_src : register(t0);

static const float w[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy * g_post.invSize;
    float3 sum = g_src.Sample(g_linearClamp, uv).rgb * w[0];
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        float2 o = g_post.direction * g_post.invSize * i;
        sum += (g_src.Sample(g_linearClamp, uv + o).rgb + g_src.Sample(g_linearClamp, uv - o).rgb) * w[i];
    }
    return float4(sum, 1.0);
}