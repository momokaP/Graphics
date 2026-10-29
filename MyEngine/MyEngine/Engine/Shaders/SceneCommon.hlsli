// SceneCommon.hlsli — 씬(Forward/Skybox) 셰이더 공용
#ifndef SCENE_COMMON_HLSLI
#define SCENE_COMMON_HLSLI

#include "ShaderInterop.h"
#include "Bindings.hlsli"

struct VSInput
{
    float3 posModel : POSITION;
    float3 normalModel : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 posProj : SV_POSITION;
    float3 posWorld : POSITION;
    float3 normalWorld : NORMAL;
    float2 texcoord : TEXCOORD;
};

#endif