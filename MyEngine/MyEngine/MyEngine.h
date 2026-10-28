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

	struct BasicVertexConstantBuffer
	{
		Matrix model;
		Matrix invTranspose;
		Matrix view;
		Matrix projection;
	};
	static_assert((sizeof(BasicVertexConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

	// 재질
	struct Material {
		Vector3 ambient = Vector3(0.0f);  // 12
		float shininess = 1.0f;           // 4
		Vector3 diffuse = Vector3(0.5f);  // 12
		float dummy1;                     // 4
		Vector3 specular = Vector3(0.5f); // 12
		float dummy2;                     // 4
		Vector3 fresnelR0 = Vector3(1.0f, 0.71f, 0.29f); // Gold
		float dummy3;
	};

	// 조명
	struct Light {
		Vector3 strength = Vector3(1.0f);              // 12
		float fallOffStart = 0.0f;                     // 4
		Vector3 direction = Vector3(0.0f, 0.0f, 1.0f); // 12
		float fallOffEnd = 10.0f;                      // 4
		Vector3 position = Vector3(0.0f, 0.0f, -2.0f); // 12
		float spotPower = 1.0f;                        // 4
	};

#define MAX_LIGHTS 3
	struct BasicPixelConstantBuffer {
		Vector3 eyeWorld;
		uint32_t useTexture;
		Material material;
		Light lights[MAX_LIGHTS]; // 48 * MAX_LIGHTS
		Vector3 rimColor = Vector3(1.0f);
		float rimPower;
		float rimStrength = 0.0f;
		uint32_t useSmoothstep = false;
		float padding[2];
	};
	static_assert((sizeof(BasicPixelConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

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

	BasicVertexConstantBuffer m_BasicVertexConstantBufferData;

	BasicPixelConstantBuffer m_BasicPixelConstantBufferData;

	int m_lightType = 0;
	Light m_lightFromGUI;
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

	BasicVertexConstantBuffer m_skyboxVertexConstantBufferData;  // 같은 구조체 타입

	static constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	UINT m_bloomWidth = 1, m_bloomHeight = 1;

	ComPtr<ID3D12RootSignature> m_postRootSignature;
	ComPtr<ID3D12PipelineState> m_brightPSO, m_blurPSO, m_combinePSO;

	struct PostConstants {          // 루트 상수 8개 (b0)
		float invSize[2];           // 그리는 대상의 1/해상도
		float direction[2];         // 블러 방향
		float threshold;
		float strength;
		float exposure;
		float pad;
	};
	static_assert(sizeof(PostConstants) % 4 == 0, "Root constants must be 32-bit aligned");
	static constexpr UINT kPostConstantCount = sizeof(PostConstants) / 4;

	float m_bloomThreshold = 1.0f;
	float m_bloomStrength = 0.1f;
	float m_exposure = 1.0f;

	int   m_bloomBlurCount = 2;

	enum MainRootParam : UINT {
		RP_VSConstants = 0,   // 루트 CBV: VS b1 (BasicVertexConstantBuffer)
		RP_PSConstants,       // 루트 CBV: PS b0 (BasicPixelConstantBuffer)
		RP_Albedo,            // 테이블: t0
		RP_EnvMap,            // 테이블: t1
		RP_Irradiance,        // 테이블: t2
		RP_Specular,          // 테이블: t3
		RP_MainCount
	};
	enum PostRootParam : UINT {
		PRP_Constants = 0,    // b0 (루트 상수)
		PRP_Source0,          // t0
		PRP_Source1,          // t1
		PRP_Count
	};

	void CreatePostTargets(UINT width, UINT height);

	DeferredReleaseQueue m_releaseQueue;

};