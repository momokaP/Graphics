#include "SceneCommon.hlsli"

PSInput main(VSInput input)
{
    PSInput output;

    float4 posWorld = mul(float4(input.posModel, 1.0f), g_object.world);
    output.posWorld = posWorld.xyz;
    output.posProj = mul(posWorld, g_frame.viewProj); // view * proj를 CPU에서 미리 곱해 둠

    output.normalWorld = normalize(mul(float4(input.normalModel, 0.0f), g_object.worldInvTranspose).xyz);
    output.texcoord = input.texcoord;
    return output;
}