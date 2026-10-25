TextureCube g_textureCube0 : register(t0);
SamplerState g_sampler : register(s1);

struct PixelShaderInput
{
    float4 posProj : SV_POSITION; // Screen position
    float3 posWorld : POSITION; // World position (조명 계산에 사용)
    float3 normalWorld : NORMAL;
    float2 texcoord : TEXCOORD;
    float3 color : COLOR; // Normal lines 쉐이더에서 사용
};

float3 ACESFilm(float3 x)   // 근사 ACES 톤매핑
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float4 main(PixelShaderInput input) : SV_TARGET
{
    float3 hdr = g_textureCube0.Sample(g_sampler, input.posWorld).rgb;

    const float exposure = 1.0; // 밝기 조절 (1~3 정도로 바꿔 보기)
    float3 color = ACESFilm(hdr * exposure); // HDR → 0~1
    color = pow(color, 1.0 / 2.2); // 선형 → 감마 (모니터용)

    return float4(color, 1.0);
}