#pragma once


#include "DXSample.h"
#include "GeometryGenerator.h"

#include <directxtk12/ResourceUploadBatch.h>
#include <directxtk12/WICTextureLoader.h>
#include <directxtk12/DDSTextureLoader.h>

#include "Vertex.h"

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

	struct SceneConstantBuffer
	{
		XMFLOAT4 offset;
		float padding[60]; // Padding so the constant buffer is 256-byte aligned.
	};
	static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

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
		Vector3 eyeWorld;         // 12
		bool useTexture;          // 4
		Material material;        // 48
		Light lights[MAX_LIGHTS]; // 48 * MAX_LIGHTS
		Vector3 rimColor = Vector3(1.0f);
		float rimPower;
		float rimStrength = 0.0f;
		bool useSmoothstep = false;
		float padding[6];
	};
	static_assert((sizeof(BasicPixelConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

	struct Mesh {
		VertexBuffer           vb;
		IndexBuffer            ib;
		ComPtr<ID3D12Resource> texture;
		UINT                   srvIndex = 3;   // 텍스처 없으면 기본 텍스처(3번 칸)
	};
	std::vector<Mesh> m_meshes;

	static constexpr UINT kModelSrvStart = 16;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12PipelineState> m_cubeMapPipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;

	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource>       m_depthStencil;
	void CreateDepthBuffer(UINT width, UINT height);

	void CreateTextureSrv(ID3D12Resource* tex, UINT heapIndex);

	/*ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;*/

	ComPtr<ID3D12Resource> m_texture;

	VertexBuffer m_vertexBuffer;
	IndexBuffer m_indexBuffer;
	
	ConstantBuffer m_constantBuffer;
	SceneConstantBuffer m_constantBufferData;

	ConstantBuffer m_BasicVertexConstantBuffer;
	BasicVertexConstantBuffer m_BasicVertexConstantBufferData;
	
	ConstantBuffer m_BasicPixelConstantBuffer;
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

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	UINT m_imguiSrvNext = 10;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();

	ComPtr<ID3D12Resource> m_cubeMapTexture;
	static constexpr UINT kCubeMapSrvIndex = 4;

	ComPtr<ID3D12Resource> m_cubeMapTextureDiffuse;
	static constexpr UINT kCubeMapSrvIndexDiffuse = 5;

	ComPtr<ID3D12Resource> m_cubeMapTextureSpecular;
	static constexpr UINT kCubeMapSrvIndexSpecular = 6;

	static constexpr UINT kSkyboxCbvStart = 7;   // 7: b0, 8: b1
	BasicVertexConstantBuffer m_skyboxVertexConstantBufferData;  // 같은 구조체 타입
	ConstantBuffer m_skyboxVertexConstantBuffer;                 // CreateConstantBuffer 반환 타입

	void CreateCubeSrv(ID3D12Resource* tex, UINT heapIndex);
};