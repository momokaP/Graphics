#pragma once

#include "DXSample.h"
#include "GeometryGenerator.h"
#include "RHI/GraphicsDevice.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/SwapChain.h"
#include "RHI/Texture.h"
#include "RHI/GpuBuffer.h"
#include "RHI/RootSignatureBuilder.h"
#include "RHI/PipelineBuilder.h"
#include "RHI/DeferredReleaseQueue.h"
#include "RHI/LinearUploadAllocator.h"

#include "Renderer/RenderTypes.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/RenderScene.h"
#include "Renderer/GpuMarker.h"
#include "Renderer/Passes/SkyboxPass.h"
#include "Renderer/Passes/ForwardPass.h"
#include "Renderer/Passes/BloomPass.h"
#include "Renderer/Passes/TonemapPass.h"
#include "Renderer/Passes/UIPass.h"

#include <directxtk12/ResourceUploadBatch.h>

#include "Vertex.h"
#include "Paths.h"

#include "Shaders/ShaderInterop.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;

class MyEngine :public DXSample
{
public:
	MyEngine(UINT width, UINT height, std::wstring name);

	void OnInit() override;
	void OnUpdate(float dt) override;
	void OnRender() override;
	void OnDestroy() override;
	void UpdateGUI() override;
	void OnSizeChanged(UINT width, UINT height, bool minimized) override;

	void InitGUI();

private:
	static const UINT FrameCount = 2;

	struct Mesh {
		GpuBuffer vb, ib;
		Texture   albedo;          // 없으면 IsValid() == false → 기본 텍스처 사용
	};
	std::vector<Mesh> m_meshes;

	Texture m_sceneColor, m_depth;
	Texture m_defaultTexture;
	Texture m_envMap, m_irradianceMap, m_specularMap;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	SwapChain m_swapChain;
	bool m_vsync = true;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	LinearUploadAllocator m_uploadAlloc;

	DescriptorHeap m_rtvHeap, m_dsvHeap, m_srvHeap;   // srv = CBV/SRV/UAV (shader-visible)

	hlsl::FrameConstants    m_frameConstants{};
	hlsl::ObjectConstants   m_modelObject{};
	hlsl::MaterialConstants m_material = hlsl::DefaultMaterial();

	int m_lightType = 0;
	hlsl::Light m_lightFromGUI = hlsl::DefaultLight();
	float m_materialDiffuse = 0.7f;
	float m_materialSpecular = 0.2f;

	Vector3 m_modelTranslation = Vector3(0.0f);
	Vector3 m_modelRotation = Vector3(-0.6f, 1.0f, 0.0f);
	Vector3 m_modelScaling = Vector3(0.5f);
	float m_viewRot = 0.0f;

	float m_projFovAngleY = 70.0f;
	float m_nearZ = 0.01f;
	float m_farZ = 100.0f;
	bool m_usePerspectiveProjection = true;

	int m_guiWidth = 0;
	float m_sceneAspect = 1.0f;
	void  UpdateSceneViewport();

	struct FrameContext
	{
		ComPtr<ID3D12CommandAllocator> allocator;   // 이 프레임의 커맨드가 기록되는 메모리
		UINT64 fenceValue = 0;                      // 이 프레임을 제출할 때 받은 펜스 값 (0 = 아직 제출 안 함)
	};
	FrameContext m_frames[FrameCount];
	UINT m_frameIndex = 0;                          // 엔진이 직접 세는 프레임 번호 (0, 1, 0, 1...)
	float m_lastGpuWaitMs = 0.0f;                   // 검증용: CPU가 GPU를 기다린 시간

	void MoveToNextFrame();
	void WaitForGpuIdle();                          // ← WaitForPreviousFrame 이름 변경

	GraphicsDevice m_gfx;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	
	float m_bloomThreshold = 1.0f;
	float m_bloomStrength = 0.1f;
	float m_exposure = 1.0f;

	int   m_bloomBlurCount = 2;

	DeferredReleaseQueue m_releaseQueue;

	SkyboxPass  m_skyboxPass;
	ForwardPass m_forwardPass;
	BloomPass   m_bloomPass;
	TonemapPass m_tonemapPass;
	UIPass      m_uiPass;
	RenderScene m_renderScene;

	RenderInitContext MakeInitContext();
	void CreateSizeDependentTargets(UINT width, UINT height);

};