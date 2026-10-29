// ShaderInterop.h — C++과 HLSL이 같이 include하는 파일
// ⚠ 규칙: float3 뒤에는 반드시 4바이트(float/uint)를 붙여 16바이트를 채울 것 (HLSL 패킹 규칙)
#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

// ───────────────────── 언어별 준비 ─────────────────────
#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
#include <directxtk12/SimpleMath.h>
#define INTEROP_BEGIN namespace hlsl {
#define INTEROP_END   }
namespace hlsl
{
    using float2 = DirectX::SimpleMath::Vector2;
    using float3 = DirectX::SimpleMath::Vector3;
    using float4 = DirectX::SimpleMath::Vector4;
    using float4x4 = DirectX::SimpleMath::Matrix;
    using uint = uint32_t;
}
#else
#pragma pack_matrix(row_major)     // 셰이더가 행렬을 C++(SimpleMath)과 같은 행 단위로 읽음 → Transpose 불필요
#define INTEROP_BEGIN
#define INTEROP_END
#endif

#define MAX_LIGHTS       3
#define NUM_DIR_LIGHTS   1
#define NUM_POINT_LIGHTS 1
#define NUM_SPOT_LIGHTS  1

INTEROP_BEGIN

struct Light                        // 48 B
{
    float3 strength;    float fallOffStart;
    float3 direction;   float fallOffEnd;
    float3 position;    float spotPower;
};

// b0: 프레임당 1번
struct FrameConstants               // 352 B
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float3   eyeWorld;   float _pad0;
    Light    lights[MAX_LIGHTS];
};

// b1: 드로우마다
struct ObjectConstants              // 128 B
{
    float4x4 world;
    float4x4 worldInvTranspose;     // 노멀 변환용
};

// b2: 머티리얼마다
struct MaterialConstants            // 80 B
{
    float3 ambient;     float shininess;
    float3 diffuse;     uint  useTexture;
    float3 specular;    float rimPower;
    float3 fresnelR0;   float rimStrength;
    float3 rimColor;    uint  useSmoothstep;
};

INTEROP_END

// ───────────────────── C++ 전용: 레이아웃 검증 + 기본값 ─────────────────────
#ifdef __cplusplus
namespace hlsl
{
    static_assert(sizeof(Light) == 48, "Light layout mismatch with HLSL");
    static_assert(offsetof(FrameConstants, eyeWorld) == 192, "FrameConstants layout");
    static_assert(offsetof(FrameConstants, lights) == 208, "lights 배열은 16B 경계에서 시작해야 함");
    static_assert(sizeof(FrameConstants) == 352, "FrameConstants layout");
    static_assert(sizeof(ObjectConstants) == 128, "ObjectConstants layout");
    static_assert(offsetof(MaterialConstants, rimColor) == 64, "MaterialConstants layout");
    static_assert(sizeof(MaterialConstants) == 80, "MaterialConstants layout");

    // HLSL은 기본값(= 1.0f 같은 멤버 초기화)을 못 쓰므로 C++ 함수로 제공
    inline Light DefaultLight()
    {
        Light l{};
        l.strength = float3(1.0f);  l.fallOffStart = 0.0f;
        l.direction = float3(0.0f, 0.0f, 1.0f);  l.fallOffEnd = 10.0f;
        l.position = float3(0.0f, 0.0f, -2.0f); l.spotPower = 1.0f;
        return l;
    }

    inline MaterialConstants DefaultMaterial()
    {
        MaterialConstants m{};
        m.ambient = float3(0.0f);  m.shininess = 1.0f;
        m.diffuse = float3(0.5f);  m.useTexture = 0;
        m.specular = float3(0.5f);  m.rimPower = 1.0f;
        m.fresnelR0 = float3(1.0f, 0.71f, 0.29f);   // Gold
        m.rimStrength = 0.0f;
        m.rimColor = float3(1.0f);  m.useSmoothstep = 0;
        return m;
    }
}
#endif

#endif // SHADER_INTEROP_H