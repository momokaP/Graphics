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
// 행렬 패킹은 ShaderCompiler의 -Zpr 인자가 정한다 (C++ SimpleMath와 같은 행 우선)
//   다른 도구로 컴파일하면 -Zpr이 빠져 행렬이 조용히 전치되므로, 그런 경우 컴파일 에러로 막음
#ifndef ENGINE_SHADER_COMPILER
#error "Compile shaders through ShaderCompiler (requires -Zpr). See ShaderCompiler.cpp."
#endif
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

// b3: 패스 파라미터 (루트 상수 8개 = 32 B)
struct PostConstants
{
    float2 invSize;      // 그리는 대상의 1/해상도
    float2 direction;    // 블러 방향
    float  threshold;
    float  strength;
    float  exposure;
    float  _pad;
};

// b4: 디버그 파라미터 (루트 상수 8개 = 32 B)
struct DebugConstants
{
    float3 tint; // 틴트 또는 와이어 프레임 색깔 
    float time; 
    float2 mouse; 
    uint objectId;
    float _pad;
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
    static_assert(sizeof(PostConstants) == 32, "PostConstants: 루트 상수 8개");
    static_assert(sizeof(DebugConstants) == 32, "DebugConstants: 루트 상수 8개");
    static_assert(offsetof(DebugConstants, objectId) == 24, "IdPass 루트 상수 오프셋");

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

 
    inline ObjectConstants MakeObjectConstants(const float4x4& world)
    {
        ObjectConstants o;
        o.world = world;
        o.worldInvTranspose = world.Invert().Transpose();
        return o;
    }
    
}
#endif

#endif // SHADER_INTEROP_H