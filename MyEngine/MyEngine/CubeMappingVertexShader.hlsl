struct VertexShaderInput
{
    float3 posModel : POSITION; //모델 좌표계의 위치 position
    float3 normalModel : NORMAL; // 모델 좌표계의 normal    
    float2 texcoord : TEXCOORD0; // <- 다음 예제에서 사용
};

struct PixelShaderInput
{
    float4 posProj : SV_POSITION; // Screen position
    float3 posWorld : POSITION; // World position (조명 계산에 사용)
    float3 normalWorld : NORMAL;
    float2 texcoord : TEXCOORD;
    float3 color : COLOR; // Normal lines 쉐이더에서 사용
};

cbuffer BasicVertexConstantBuffer : register(b1)
{
    matrix model;
    matrix invTranspose;
    matrix view;
    matrix projection;
};

PixelShaderInput main(VertexShaderInput input)
{
    
    PixelShaderInput output;
    float4 pos = float4(input.posModel, 1.0f);

    pos = mul(pos, model); // Identity

    output.posWorld = pos.xyz;
    
    float4 normal = float4(input.normalModel, 0.0f);
    output.normalWorld = mul(normal, invTranspose).xyz;
    output.normalWorld = normalize(output.normalWorld);

    pos = mul(pos, view);
    
    pos = mul(pos, projection);
    output.posProj = pos;

    output.texcoord = input.texcoord;
    output.color = float3(1.0, 1.0, 0.0);

    return output;
}
