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

	Texture m_sceneColor, m_bloom[2], m_depth;
	Texture m_defaultTexture;
	Texture m_envMap, m_irradianceMap, m_specularMap;
	GpuBuffer m_skyboxVB, m_skyboxIB;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	SwapChain m_swapChain;
	bool m_vsync = true;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12PipelineState> m_cubeMapPipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	LinearUploadAllocator m_uploadAlloc;

	DescriptorHeap m_rtvHeap, m_dsvHeap, m_srvHeap;   // srv = CBV/SRV/UAV (shader-visible)

	hlsl::FrameConstants    m_frameConstants{};
	hlsl::ObjectConstants   m_modelObject{};
	hlsl::ObjectConstants   m_skyboxObject{};
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

	static constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	UINT m_bloomWidth = 1, m_bloomHeight = 1;

	ComPtr<ID3D12PipelineState> m_brightPSO, m_blurPSO, m_combinePSO;
	
	static constexpr UINT kPostConstantCount = sizeof(hlsl::PostConstants) / 4;

	float m_bloomThreshold = 1.0f;
	float m_bloomStrength = 0.1f;
	float m_exposure = 1.0f;

	int   m_bloomBlurCount = 2;

	// 엔진 공용 루트 시그니처 슬롯 — 순서는 Bindings.hlsli의 레지스터 규약과 대응
	enum RootParam : UINT {
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

	void CreatePostTargets(UINT width, UINT height);

	DeferredReleaseQueue m_releaseQueue;

};