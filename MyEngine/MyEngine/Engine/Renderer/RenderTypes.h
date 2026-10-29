#pragma once
#include <d3d12.h>
#include "Shaders/ShaderInterop.h"

// 엔진 공용 루트 시그니처 슬롯 — 순서는 Bindings.hlsli의 레지스터 규약과 대응
enum RootParam : UINT
{
    RP_Frame = 0,        // CBV b0 (ALL)
    RP_Object,           // CBV b1 (VS)
    RP_Material,         // CBV b2 (PS)
    RP_PassConstants,    // 루트 상수 ×8, b3 (PS)
    RP_Tex0,             // t0  패스 입력 / 머티리얼
    RP_Tex1,             // t1
    RP_Tex2,             // t2
    RP_Tex3,             // t3
    RP_EnvMap,           // t4  전역
    RP_Irradiance,       // t5  전역
    RP_Specular,         // t6  전역
    RP_Count
};

inline constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
inline constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;
inline constexpr UINT kPostConstantCount = sizeof(hlsl::PostConstants) / 4;