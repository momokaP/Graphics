#pragma once
#include <vector>
#include <d3d12.h>
#include "Shaders/ShaderInterop.h"

class GpuBuffer;
class Texture;

// 드로우 1번에 필요한 것
struct DrawItem
{
    const GpuBuffer* vertexBuffer = nullptr;
    const GpuBuffer* indexBuffer = nullptr;
    const Texture* albedo = nullptr;   // 없으면 기본 텍스처를 넣어서 전달
    hlsl::ObjectConstants object{};
};

struct PostSettings
{
    float exposure = 1.0f;
    float bloomThreshold = 1.0f;
    float bloomStrength = 0.1f;
    int   bloomBlurCount = 2;
};

// 렌더러가 한 프레임을 그리는 데 필요한 모든 입력 (앱이 매 프레임 채움)
struct RenderScene
{
    hlsl::FrameConstants    frame{};
    hlsl::MaterialConstants material{};
    std::vector<DrawItem>   draws;

    const Texture* envMap = nullptr;
    const Texture* irradianceMap = nullptr;
    const Texture* specularMap = nullptr;

    PostSettings post;

    D3D12_VIEWPORT viewport{};   // 씬이 그려질 화면 영역 (GUI 패널 제외)
    D3D12_RECT     scissor{};
};