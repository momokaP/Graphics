#include "stdafx.h"
#include "MyEngine.h"
#include <filesystem>

MyEngine::MyEngine(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0),
	m_constantBufferData(),
	m_BasicVertexConstantBufferData(),
	m_BasicPixelConstantBufferData()
{
}

void MyEngine::OnInit()
{
	LoadPipeline();
	LoadAssets();
	InitGUI();
}

void MyEngine::UpdateSceneViewport()
{
	float sceneWidth = float(m_width) - float(m_guiWidth);
	if (sceneWidth < 1.0f || m_height == 0)
		return;   // GUI가 창 전체를 덮거나 최소화된 경우

	m_viewport = CD3DX12_VIEWPORT(float(m_guiWidth), 0.0f, sceneWidth, float(m_height));
	m_scissorRect = CD3DX12_RECT(m_guiWidth, 0, LONG(m_width), LONG(m_height));
	m_sceneAspect = sceneWidth / float(m_height);
}

void MyEngine::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
		));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
		));
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
	
	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
	
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.NumDescriptors = 128;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
	
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;   // DSV 힙은 shader visible 불가
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
	
		CreateDepthBuffer(m_width, m_height);
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void MyEngine::LoadAssets()
{
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
		CD3DX12_ROOT_PARAMETER1 rootParameters[3];

		// VS: 힙 0,1번 칸 → b0, b1
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		// PS: 힙 2번 칸 → b0
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	{
		UINT8* pVertexShaderData = nullptr;
		UINT8* pPixelShaderData = nullptr;
		UINT vertexShaderDataLength = 0;
		UINT pixelShaderDataLength = 0;

		ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"VertexShader.cso").c_str(), &pVertexShaderData, &vertexShaderDataLength));
		ThrowIfFailed(ReadDataFromFile(GetAssetFullPath(L"PixelShader.cso").c_str(), &pPixelShaderData, &pixelShaderDataLength));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength);
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // Depth ON, LESS, Write ALL
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		/*psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;*/
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	ThrowIfFailed(m_commandList->Close());

	{
		std::vector<MeshData> meshes =
			GeometryGenerator::ReadFromFile("c:/Users/9451s/OneDrive/Documents/zaku2/", "zaku2.fbx");

		MeshData meshData = GeometryGenerator::MakeBox();

		// MeshData meshData = GeometryGenerator::MakeCylinder(1.0f, 1.0f, 1.0f, 10);
		
		/*MeshData meshData = GeometryGenerator::MakeSphere(1.5f, 12, 12);
		meshData = GeometryGenerator::SubdivideToSphere(1.5f, meshData);
		meshData = GeometryGenerator::SubdivideToSphere(1.5f, meshData);
		meshData = GeometryGenerator::SubdivideToSphere(1.5f, meshData);*/

		// MeshData meshData = GeometryGenerator::MakeIcosahedron();

		m_vertexBuffer = CreateVertexBuffer(m_device.Get(), meshData.vertices);
		m_indexBuffer = CreateIndexBuffer(m_device.Get(), meshData.indices);
		
		UINT cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
		
		m_constantBuffer = CreateConstantBuffer(m_device.Get(), m_constantBufferData, 
			cbvHandle);

		cbvHandle.Offset(1, cbvDescriptorSize);
		m_BasicVertexConstantBuffer = CreateConstantBuffer(m_device.Get(), m_BasicVertexConstantBufferData,
			cbvHandle);

		cbvHandle.Offset(1, cbvDescriptorSize);
		m_BasicPixelConstantBuffer = CreateConstantBuffer(m_device.Get(), m_BasicPixelConstantBufferData,
			cbvHandle);
	}

	// Create the texture.
	{
		const std::string modelDir = "c:/Users/9451s/OneDrive/Documents/zaku2/";
		std::vector<MeshData> meshDatas = GeometryGenerator::ReadFromFile(modelDir, "zaku2.fbx");

		DirectX::ResourceUploadBatch upload(m_device.Get());
		upload.Begin();

		// 기본 텍스처 (3번 칸) — 기존 코드 그대로
		ThrowIfFailed(DirectX::CreateWICTextureFromFile(m_device.Get(), upload,
			GetAssetFullPath(L"kotone.png").c_str(), m_texture.ReleaseAndGetAddressOf()));

		UINT inc = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		UINT nextSrv = kModelSrvStart;

		for (const MeshData& md : meshDatas) {
			Mesh mesh;
			mesh.vb = CreateVertexBuffer(m_device.Get(), md.vertices);
			mesh.ib = CreateIndexBuffer(m_device.Get(), md.indices);

			if (!md.albedoTexturePath.empty() && std::filesystem::exists(md.albedoTexturePath)) {
				std::wstring wpath = std::filesystem::path(md.albedoTexturePath).wstring();
				if (SUCCEEDED(DirectX::CreateWICTextureFromFile(m_device.Get(), upload,
					wpath.c_str(), mesh.texture.ReleaseAndGetAddressOf()))) {
					mesh.srvIndex = nextSrv++;
				}
			}
			m_meshes.push_back(std::move(mesh));
		}

		upload.End(m_commandQueue.Get()).wait();

		// SRV 생성 (업로드가 끝난 뒤 리소스 기준)
		CreateTextureSrv(m_texture.Get(), 3);
		for (const Mesh& mesh : m_meshes)
			if (mesh.texture) CreateTextureSrv(mesh.texture.Get(), mesh.srvIndex);
	}

	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		WaitForPreviousFrame();
	}
}

void MyEngine::CreateTextureSrv(ID3D12Resource* tex, UINT heapIndex)
{
	UINT inc = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE h(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), heapIndex, inc);

	D3D12_RESOURCE_DESC d = tex->GetDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Format = d.Format;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MipLevels = d.MipLevels;
	m_device->CreateShaderResourceView(tex, &srv, h);
}

void MyEngine::InitGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(Win32Application::GetHwnd());

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = m_device.Get();
	initInfo.CommandQueue = m_commandQueue.Get();
	initInfo.NumFramesInFlight = FrameCount;
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	initInfo.SrvDescriptorHeap = m_cbvHeap.Get();
	initInfo.UserData = this;
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
		{
			auto* self = static_cast<MyEngine*>(info->UserData);
			UINT inc = self->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			UINT idx = self->m_imguiSrvNext++;
			*outCpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), idx, inc);
			*outGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), idx, inc);
		};
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*,
		D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			// 지금은 단순 증가 할당이라 해제는 생략 (나중에 프리리스트로 교체)
		};

	ImGui_ImplDX12_Init(&initInfo);
}

void MyEngine::OnUpdate(float dt)
{
	using namespace DirectX;

	// 모델의 변환
	m_BasicVertexConstantBufferData.model =
		Matrix::CreateScale(m_modelScaling) *
		Matrix::CreateRotationY(m_modelRotation.y) *
		Matrix::CreateRotationX(m_modelRotation.x) *
		Matrix::CreateRotationZ(m_modelRotation.z) *
		Matrix::CreateTranslation(m_modelTranslation);
	m_BasicVertexConstantBufferData.model =
		m_BasicVertexConstantBufferData.model.Transpose();

	m_BasicVertexConstantBufferData.invTranspose =
		m_BasicVertexConstantBufferData.model;
	m_BasicVertexConstantBufferData.invTranspose.Translation(Vector3(0.0f));
	m_BasicVertexConstantBufferData.invTranspose =
		m_BasicVertexConstantBufferData.invTranspose.Transpose().Invert();

	// 시점 변환
	m_BasicVertexConstantBufferData.view =
		Matrix::CreateRotationY(m_viewRot) *
		Matrix::CreateTranslation(0.0f, 0.0f, 2.0f);

	m_BasicPixelConstantBufferData.eyeWorld = Vector3::Transform(
		Vector3(0.0f), m_BasicVertexConstantBufferData.view.Invert());

	m_BasicVertexConstantBufferData.view =
		m_BasicVertexConstantBufferData.view.Transpose();

	// 프로젝션
	const float aspect = m_sceneAspect; // <- GUI에서 조절
	if (m_usePerspectiveProjection) {
		m_BasicVertexConstantBufferData.projection = XMMatrixPerspectiveFovLH(
			XMConvertToRadians(m_projFovAngleY), aspect, m_nearZ, m_farZ);
	}
	else {
		m_BasicVertexConstantBufferData.projection =
			XMMatrixOrthographicOffCenterLH(-aspect, aspect, -1.0f, 1.0f,
				m_nearZ, m_farZ);
	}
	m_BasicVertexConstantBufferData.projection =
		m_BasicVertexConstantBufferData.projection.Transpose();

	memcpy(m_BasicVertexConstantBuffer.mapped, &m_BasicVertexConstantBufferData, sizeof(m_BasicVertexConstantBufferData));

	m_BasicPixelConstantBufferData.material.diffuse =
		Vector3(m_materialDiffuse);
	m_BasicPixelConstantBufferData.material.specular =
		Vector3(m_materialSpecular);

	// 여러 개 조명 사용 예시
	for (int i = 0; i < MAX_LIGHTS; i++) {
		// 다른 조명 끄기
		if (i != m_lightType) {
			m_BasicPixelConstantBufferData.lights[i].strength *= 0.0f;
		}
		else {
			m_BasicPixelConstantBufferData.lights[i] = m_lightFromGUI;
		}
	}

	memcpy(m_BasicPixelConstantBuffer.mapped, &m_BasicPixelConstantBufferData, sizeof(m_BasicPixelConstantBufferData));

}

void MyEngine::OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void MyEngine::OnDestroy()
{
	WaitForPreviousFrame();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CloseHandle(m_fenceEvent);
}

void MyEngine::PopulateCommandList()
{
	ThrowIfFailed(m_commandAllocator->Reset());

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	UINT cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);
	
	gpuHandle.Offset(2, cbvDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

	m_commandList->RSSetViewports(1, &m_viewport);
	
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	auto toRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &toRenderTarget);
 	
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.2f, 0.2f, 0.25f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CD3DX12_GPU_DESCRIPTOR_HANDLE heapStart(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	for (const Mesh& mesh : m_meshes) {
		m_commandList->SetGraphicsRootDescriptorTable(2,
			CD3DX12_GPU_DESCRIPTOR_HANDLE(heapStart, mesh.srvIndex, cbvDescriptorSize));
		m_commandList->IASetVertexBuffers(0, 1, &mesh.vb.view);
		m_commandList->IASetIndexBuffer(&mesh.ib.view);
		m_commandList->DrawIndexedInstanced(mesh.ib.count, 1, 0, 0, 0);
	}

	// ImGui: 박스 다음, Present 배리어 전
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		m_renderTargets[m_frameIndex].Get(), 
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &toPresent);

	ThrowIfFailed(m_commandList->Close());
}

void MyEngine::WaitForPreviousFrame()
{
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void MyEngine::UpdateGUI() 
{
	ImGui::Checkbox("Use Texture", &m_BasicPixelConstantBufferData.useTexture);
	ImGui::SliderFloat3("m_modelTranslation", &m_modelTranslation.x, -2.0f,
		2.0f);
	ImGui::SliderFloat3("m_modelRotation", &m_modelRotation.x, -3.14f, 3.14f);
	ImGui::SliderFloat3("m_modelScaling", &m_modelScaling.x, 0.1f, 2.0f);
	ImGui::SliderFloat("m_viewRot", &m_viewRot, -3.14f, 3.14f);

	ImGui::SliderFloat("Material Shininess",
		&m_BasicPixelConstantBufferData.material.shininess, 1.0f,
		256.0f);

	if (ImGui::RadioButton("Directional Light", m_lightType == 0)) {
		m_lightType = 0;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Point Light", m_lightType == 1)) {
		m_lightType = 1;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Spot Light", m_lightType == 2)) {
		m_lightType = 2;
	}

	ImGui::SliderFloat("Material Diffuse", &m_materialDiffuse, 0.0f, 1.0f);
	ImGui::SliderFloat("Material Specular", &m_materialSpecular, 0.0f, 1.0f);

	ImGui::SliderFloat3("Light Position", &m_lightFromGUI.position.x, -5.0f,
		5.0f);

	ImGui::SliderFloat("Light fallOffStart", &m_lightFromGUI.fallOffStart, 0.0f,
		5.0f);

	ImGui::SliderFloat("Light fallOffEnd", &m_lightFromGUI.fallOffEnd, 0.0f,
		10.0f);

	ImGui::SliderFloat("Light spotPower", &m_lightFromGUI.spotPower, 1.0f,
		512.0f);

	int newWidth = int(ImGui::GetWindowWidth());
	if (newWidth != m_guiWidth)
	{
		m_guiWidth = newWidth;
		UpdateSceneViewport();   // OnUpdate 전에 aspect까지 확정
	}
}

void MyEngine::OnSizeChanged(UINT width, UINT height, bool minimized)
{
	// 초기화 전이거나, 최소화 / 크기 0이면 무시
	if (!m_swapChain || minimized || width == 0 || height == 0)
		return;

	// 1. GPU가 현재 백버퍼를 다 쓸 때까지 대기
	WaitForPreviousFrame();

	// 2. 백버퍼 참조 해제 (DX11의 m_renderTargetView.Reset()에 해당)
	for (UINT n = 0; n < FrameCount; n++)
		m_renderTargets[n].Reset();

	// 3. 스왑체인 버퍼 크기 변경
	DXGI_SWAP_CHAIN_DESC desc = {};
	m_swapChain->GetDesc(&desc);
	ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, width, height,
		desc.BufferDesc.Format, desc.Flags));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// 4. RTV를 같은 힙 칸에 다시 생성 (힙은 재생성 불필요)
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT n = 0; n < FrameCount; n++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}

	// 5. 크기 의존 상태 갱신
	UpdateForSizeChange(width, height);   // m_width, m_height, m_aspectRatio
	UpdateSceneViewport();

	// 깊이 버퍼를 붙였다면 여기서 재생성
	m_depthStencil.Reset();
	CreateDepthBuffer(width, height);
}

void MyEngine::CreateDepthBuffer(UINT width, UINT height)
{
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT, width, height, 1, 1, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
		IID_PPV_ARGS(&m_depthStencil)));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc,
		m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}
