// Bindings.hlsli — 엔진 공용 레지스터 규약 (C++ RootParam enum과 1:1 대응)
//   b0 Frame | b1 Object | b2 Material | b3 Pass(루트 상수)
//   t0~t3 패스 입력 / 머티리얼 텍스처 (셰이더마다 각자 선언)
//   t4 Env | t5 Irradiance | t6 Specular (엔진 전역, 여기서 선언)
//   s0 PointClamp | s1 LinearClamp | s2 LinearWrap | s3 AnisoWrap
#ifndef BINDINGS_HLSLI
#define BINDINGS_HLSLI

#include "ShaderInterop.h"

ConstantBuffer<FrameConstants> g_frame : register(b0);
ConstantBuffer<ObjectConstants> g_object : register(b1);
ConstantBuffer<MaterialConstants> g_material : register(b2);
ConstantBuffer<PostConstants> g_post : register(b3); // 루트 상수도 셰이더에선 상수 버퍼로 보임

TextureCube g_envMap : register(t4);
TextureCube g_irradianceMap : register(t5);
TextureCube g_specularMap : register(t6);

SamplerState g_pointClamp : register(s0);
SamplerState g_linearClamp : register(s1);
SamplerState g_linearWrap : register(s2);
SamplerState g_anisoWrap : register(s3);

#endif