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
	void BuildRenderScene();          // 앱 데이터 → RenderScene (추출 단계)

	// ── 렌더러 ── (에셋보다 먼저 선언 → 에셋보다 나중에 소멸)
	Renderer    m_renderer;
	RenderScene m_renderScene;

	// ── 에셋 (3단계 AssetManager로 이동 예정) ──
	struct Mesh
	{
		GpuBuffer vb, ib;
		Texture   albedo;     // 없으면 IsValid() == false → 기본 텍스처 사용
	};
	std::vector<Mesh> m_meshes;
	Texture m_defaultTexture;
	Texture m_envMap, m_irradianceMap, m_specularMap;

	// ── 씬 데이터 (3단계 Scene으로 이동 예정) ──
	hlsl::FrameConstants    m_frameConstants{};
	hlsl::ObjectConstants   m_modelObject{};
	hlsl::MaterialConstants m_material = hlsl::DefaultMaterial();

	int         m_lightType = 0;
	hlsl::Light m_lightFromGUI = hlsl::DefaultLight();
	float m_materialDiffuse  = 0.7f;
	float m_materialSpecular = 0.2f;

	Vector3 m_modelTranslation = Vector3(0.0f);
	Vector3 m_modelRotation    = Vector3(-0.6f, 1.0f, 0.0f);
	Vector3 m_modelScaling     = Vector3(0.5f);
	
	Camera m_camera;
	bool  m_usePerspectiveProjection = true;
	
	bool  m_wireframe = false;
	Vector3 m_tint = Vector3(1.0f, 1.0f, 1.0f);

	// ── 후처리 설정 ──
	float m_exposure       = 1.0f;
	float m_bloomThreshold = 1.0f;
	float m_bloomStrength  = 0.1f;
	int   m_bloomBlurCount = 2;

	// ── 화면 영역 (GUI 패널 제외) ──
	int            m_guiWidth = 0;
	float          m_sceneAspect = 1.0f;
	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT     m_scissorRect{};
};
