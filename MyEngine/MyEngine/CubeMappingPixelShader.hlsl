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

float4 main(PixelShaderInput input) : SV_TARGET
{
    // 주의: 텍스춰 좌표가 float3 입니다.
    return g_textureCube0.Sample(g_sampler, input.posWorld.xyz);
}