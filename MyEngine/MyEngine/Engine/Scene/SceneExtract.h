#pragma once
#include <d3d12.h>
#include "Shaders/ShaderInterop.h"

struct Scene;
class AssetManager;
struct RenderScene;

// "씬을 어떻게 보는가" — 같은 Scene을 여러 뷰로 그릴 수 있도록 Scene과 분리
struct SceneView
{
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissor{};
	bool perspective = true;

	bool wireframe = false;
	hlsl::float3 wireColor{ 1.0f, 1.0f, 1.0f };
};

// Scene + Assets → RenderScene (렌더러 입력 스냅샷)
//   - 핸들 → 포인터 변환은 여기서만
//   - Scene과 Renderer는 서로를 모르고, 이 함수만 둘을 안다
void ExtractRenderScene(const Scene& scene, const AssetManager& assets,
	const SceneView& view, RenderScene& out);