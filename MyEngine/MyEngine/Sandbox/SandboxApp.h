#pragma once

// SandboxApp — 2-5 버전 앱
//   렌더링 인프라(디바이스, 스왑체인, 프레임, 패스)는 Renderer가 맡고,
//   앱은 "무엇을 그릴지(RenderScene)"만 채운다.
//   기존 MyEngine(2-4 구조)과 비교할 수 있도록 별도 클래스로 둔다.
//   → main.cpp의 USE_SANDBOX_APP으로 전환

#include <vector>

#include "DXSample.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderScene.h"
#include "RHI/Texture.h"
#include "RHI/GpuBuffer.h"
#include "Shaders/ShaderInterop.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Assets/AssetManager.h"
#include "Scene/SceneExtract.h"
#include "Editor/EditorUI.h"

class SandboxApp : public DXSample
{
public:
	SandboxApp(UINT width, UINT height, std::wstring name);

	void OnInit() override;
	void OnUpdate(float dt) override;
	void OnRender() override;
	void OnDestroy() override;
	void UpdateGUI() override;
	void OnSizeChanged(UINT width, UINT height, bool minimized) override;

private:
	// 헤더에서 전역 using을 쓰지 않도록 클래스 안에서만 별칭
	using Matrix  = DirectX::SimpleMath::Matrix;
	using Vector3 = DirectX::SimpleMath::Vector3;

	void LoadAssets();
	void InitGUI();
	void UpdateSceneViewport();

	// ── 렌더러 ── (에셋보다 먼저 선언 → 에셋보다 나중에 소멸)
	Renderer    m_renderer;
	AssetManager m_assets; // ⚠ Renderer 다음에 선언 → 먼저 소멸. Renderer::Shutdown이 GPU를 이미 비운 뒤라 안전
	RenderScene m_renderScene;

	// ── 씬 데이터 (3단계 Scene으로 이동 예정) ──
	Scene m_scene;
	SceneView m_view;
	EditorUI m_editor;

	// ── 화면 영역 (GUI 패널 제외) ──
	int            m_guiWidth = 0;
	float          m_sceneAspect = 1.0f;
	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT     m_scissorRect{};
};
