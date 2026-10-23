cbuffer PostConstants : register(b0)
{
    float2 invSize; // 지금 그리는 대상의 1/해상도
    float2 direction; // 블러 방향 (1,0) / (0,1)
    float threshold;
    float strength;
    float2 _pad;
};

SamplerState g_linear : register(s0);