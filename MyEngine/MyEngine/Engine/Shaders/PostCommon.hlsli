cbuffer PostConstants : register(b0)
{
    float2 invSize;
    float2 direction;
    float threshold;
    float strength;
    float exposure;
    float _pad;
};

SamplerState g_linear : register(s0);

float3 ACESFilm(float3 x)   // 근사 ACES 톤매핑 (Narkowicz)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}