cbuffer SceneConstanctBuffer : register(b0)
{
    float4 offset;
    float4 padding[15];
};

cbuffer BasicVertexConstantBuffer : register(b1)
{
    float4x4 model;
    float4x4 invTranspose;
    float4x4 view;
    float4x4 projection;
};

struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 pos = input.position;
    pos = mul(pos, model);
    
    output.position = pos; // 월드 위치 따로 저장

    pos = mul(pos, view);
    pos = mul(pos, projection);

    output.position = pos;
    
    output.color = input.color; // 다른 쉐이더에서 사용

    return output;
    
    //PSInput result;

    //result.position = input.position + offset;
    //result.color = input.color;

    //return result;
}