#include "stdafx.h"
#include "MyEngine.h"
#include <filesystem>

MyEngine::MyEngine(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_constantBufferData(),
	m_BasicVertexConstantBufferData(),
	m_BasicPixelConstantBufferData()
{
}

void MyEngine::OnInit()
{
	Paths::Init();
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
	m_gfx.Init(m_useWarpDevice);
	m_device = m_gfx.Device();
	m_commandQueue = m_gfx.Queue();

	m_rtvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16, false, L"RTV Heap");
	m_dsvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 4, false, L"DSV Heap");
	m_srvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true, L"CBV/SRV Heap");

	SwapChain::Desc scDesc;
	scDesc.width = m_width;
	scDesc.height = m_height;
	scDesc.bufferCount = FrameCount;
	scDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_swapChain.Init(m_gfx, m_rtvHeap, Win32Application::GetHwnd(), scDesc);
	m_frameIndex = m_swapChain.CurrentIndex();

	m_depth.CreateDepth(m_device.Get(), m_dsvHeap, m_width, m_height, DXGI_FORMAT_D32_FLOAT, L"SceneDepth");

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

static const float kSceneClear[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

void MyEngine::CreatePostTargets(UINT width, UINT height)
{
	static const float kBlack[4] = { 0, 0, 0, 1 };
	m_sceneColor.CreateRenderTarget(m_device.Get(), m_rtvHeap, m_srvHeap,
		width, height, kHdrFormat, kSceneClear, L"SceneColor");

	m_bloomWidth = max(1u, width / 2);
	m_bloomHeight = max(1u, height / 2);
	m_bloom[0].CreateRenderTarget(m_device.Get(), m_rtvHeap, m_srvHeap,
		m_bloomWidth, m_bloomHeight, kHdrFormat, kBlack, L"Bloom[0]");
	m_bloom[1].CreateRenderTarget(m_device.Get(), m_rtvHeap, m_srvHeap,
		m_bloomWidth, m_bloomHeight, kHdrFormat, kBlack, L"Bloom[1]");
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

		CD3DX12_DESCRIPTOR_RANGE1 ranges[6];
		CD3DX12_ROOT_PARAMETER1 rootParameters[6];

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

		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);

		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[4].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);

		ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0,
			D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[5].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_PIXEL);


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

		D3D12_STATIC_SAMPLER_DESC samplers[2] = { sampler, sampler };
		samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[1].ShaderRegister = 1;   // s1

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, _countof(samplers), samplers, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	{
		auto vs = LoadShader(GetAssetFullPath(L"VertexShader.cso"));
		auto ps = LoadShader(GetAssetFullPath(L"PixelShader.cso"));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = ToBytecode(vs);
		psoDesc.PS = ToBytecode(ps);
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
		psoDesc.RTVFormats[0] = kHdrFormat;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// ── 후처리 루트 시그니처 ──
	{
		CD3DX12_DESCRIPTOR_RANGE1 r[2];
		r[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE); // t0
		r[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE); // t1

		CD3DX12_ROOT_PARAMETER1 p[3];
		p[0].InitAsConstants(sizeof(PostConstants) / 4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);   // b0
		p[1].InitAsDescriptorTable(1, &r[0], D3D12_SHADER_VISIBILITY_PIXEL);
		p[2].InitAsDescriptorTable(1, &r[1], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC s = {};
		s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;   // 다운/업샘플에 bilinear가 필요
		s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		s.MaxLOD = D3D12_FLOAT32_MAX;
		s.ShaderRegister = 0;
		s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rs;
		rs.Init_1_1(_countof(p), p, 1, &s, D3D12_ROOT_SIGNATURE_FLAG_NONE);   // 입력 레이아웃 없음

		ComPtr<ID3DBlob> sig, err;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1_1, &sig, &err));
		ThrowIfFailed(m_device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
			IID_PPV_ARGS(&m_postRootSignature)));
	}

	// ── 후처리 PSO 3개 ──
	{
		auto vs = LoadShader(GetAssetFullPath(L"FullscreenVS.cso"));

		auto makePostPSO = [&](const wchar_t* psFile, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat,
			ComPtr<ID3D12PipelineState>& out)
			{
				auto ps = LoadShader(GetAssetFullPath(psFile));

				D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
				d.pRootSignature = m_postRootSignature.Get();
				d.VS = ToBytecode(vs);
				d.PS = ToBytecode(ps);
				d.InputLayout = { nullptr, 0 };
				d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				d.DepthStencilState.DepthEnable = FALSE;
				d.DSVFormat = dsvFormat;
				d.SampleMask = UINT_MAX;
				d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				d.NumRenderTargets = 1;
				d.RTVFormats[0] = rtvFormat;
				d.SampleDesc.Count = 1;
				ThrowIfFailed(m_device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&out)));
			};

		makePostPSO(L"BloomBrightPS.cso", kHdrFormat, DXGI_FORMAT_UNKNOWN, m_brightPSO);
		makePostPSO(L"BloomBlurPS.cso", kHdrFormat, DXGI_FORMAT_UNKNOWN, m_blurPSO);
		makePostPSO(L"BloomCombinePS.cso", m_swapChain.Format(), DXGI_FORMAT_D32_FLOAT, m_combinePSO);
	}

	{
		auto vs = LoadShader(GetAssetFullPath(L"CubeMappingVertexShader.cso"));
		auto ps = LoadShader(GetAssetFullPath(L"CubeMappingPixelShader.cso"));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = ToBytecode(vs);
		psoDesc.PS = ToBytecode(ps);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // Depth ON, LESS, Write ALL
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = kHdrFormat;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_cubeMapPipelineState)));
	}

	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	ThrowIfFailed(m_commandList->Close());

	CreatePostTargets(m_width, m_height);

	{
		DirectX::ResourceUploadBatch upload(m_device.Get());
		upload.Begin();

		// 스카이박스용 박스
		MeshData box = GeometryGenerator::MakeBox();
		m_skyboxVB.CreateVertexBuffer(m_device.Get(), upload, box.vertices, L"Skybox VB");
		m_skyboxIB.CreateIndexBuffer(m_device.Get(), upload, box.indices, L"Skybox IB");

		// 기본 텍스처
		m_defaultTexture.LoadWIC(m_device.Get(), upload, Paths::Asset(L"Textures/kotone.png"), true);

		// 모델
		auto meshDatas = GeometryGenerator::ReadFromFile(Paths::Asset(L"Models/zaku2/zaku2_1.fbx"));
		m_meshes.resize(meshDatas.size());     // ⚠ resize 먼저: push_back 중 재할당되면 업로드 대기 중인 리소스 포인터가 꼬일 수 있음
		for (size_t i = 0; i < meshDatas.size(); ++i)
		{
			const MeshData& md = meshDatas[i];
			Mesh& mesh = m_meshes[i];
			mesh.vb.CreateVertexBuffer(m_device.Get(), upload, md.vertices, L"Mesh VB");
			mesh.ib.CreateIndexBuffer(m_device.Get(), upload, md.indices, L"Mesh IB");
			if (!md.albedoTexturePath.empty() && std::filesystem::exists(md.albedoTexturePath))
				mesh.albedo.LoadWIC(m_device.Get(), upload, md.albedoTexturePath, true);
		}

		// IBL 큐브맵
		const auto ibl = Paths::Asset(L"DayEnvironmentHDRI057Brdf");
		m_envMap.LoadDDS(m_device.Get(), upload, ibl / L"DayEnvironmentHDRI057EnvHDR.dds");
		m_irradianceMap.LoadDDS(m_device.Get(), upload, ibl / L"DayEnvironmentHDRI057DiffuseHDR.dds");
		m_specularMap.LoadDDS(m_device.Get(), upload, ibl / L"DayEnvironmentHDRI057SpecularHDR.dds");

		upload.End(m_commandQueue.Get()).wait();

		// SRV는 업로드가 끝난 뒤 생성
		m_defaultTexture.CreateSrv(m_device.Get(), m_srvHeap);
		for (Mesh& mesh : m_meshes)
			if (mesh.albedo.IsValid()) mesh.albedo.CreateSrv(m_device.Get(), m_srvHeap);
		m_envMap.CreateSrv(m_device.Get(), m_srvHeap);
		m_irradianceMap.CreateSrv(m_device.Get(), m_srvHeap);
		m_specularMap.CreateSrv(m_device.Get(), m_srvHeap);
	}

	{
		m_vsCbvTable = m_srvHeap.Allocate(2);   // RP_VSConstants 테이블: b0, b1
		m_constantBuffer = CreateConstantBuffer(m_device.Get(), m_constantBufferData,
			m_srvHeap.Cpu(m_vsCbvTable.index + 0));
		m_BasicVertexConstantBuffer = CreateConstantBuffer(m_device.Get(), m_BasicVertexConstantBufferData,
			m_srvHeap.Cpu(m_vsCbvTable.index + 1));

		m_psCbv = m_srvHeap.Allocate();          // RP_PSConstants 테이블: b0
		m_BasicPixelConstantBuffer = CreateConstantBuffer(m_device.Get(), m_BasicPixelConstantBufferData,
			m_psCbv.cpu);

		// 스카이박스: b0은 위 m_constantBuffer를 같이 가리키고, b1은 스카이박스 전용
		m_skyboxVsCbvTable = m_srvHeap.Allocate(2);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
		cbv.BufferLocation = m_constantBuffer.resource->GetGPUVirtualAddress();
		cbv.SizeInBytes = m_constantBuffer.size;
		m_device->CreateConstantBufferView(&cbv, m_srvHeap.Cpu(m_skyboxVsCbvTable.index + 0));

		m_skyboxVertexConstantBuffer = CreateConstantBuffer(m_device.Get(), m_skyboxVertexConstantBufferData,
			m_srvHeap.Cpu(m_skyboxVsCbvTable.index + 1));
	}

	{
		WaitForPreviousFrame();
	}
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
	initInfo.RTVFormat = m_swapChain.Format();
	initInfo.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	initInfo.SrvDescriptorHeap = m_srvHeap.Heap();
	initInfo.UserData = &m_srvHeap;
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
		{
			DescriptorHandle h = static_cast<DescriptorHeap*>(info->UserData)->Allocate();
			*outCpu = h.cpu;
			*outGpu = h.gpu;
		};
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			static_cast<DescriptorHeap*>(info->UserData)->Free(cpu);
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

	m_skyboxVertexConstantBufferData = m_BasicVertexConstantBufferData;
	m_skyboxVertexConstantBufferData.model = Matrix::CreateScale(20.0f).Transpose();
	m_skyboxVertexConstantBufferData.invTranspose = Matrix();  // 스카이박스는 노멀 안 씀
	memcpy(m_skyboxVertexConstantBuffer.mapped, &m_skyboxVertexConstantBufferData,
		sizeof(m_skyboxVertexConstantBufferData));
}

void MyEngine::OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_swapChain.Present(m_vsync);

	WaitForPreviousFrame();
}

void MyEngine::OnDestroy()
{
	WaitForPreviousFrame();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_gfx.Shutdown();
}

void MyEngine::PopulateCommandList()
{
	ThrowIfFailed(m_commandAllocator->Reset());

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Heap()};
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->SetGraphicsRootDescriptorTable(0, m_vsCbvTable.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(1, m_psCbv.gpu);

	m_commandList->RSSetViewports(1, &m_viewport);
	
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	m_sceneColor.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->OMSetRenderTargets(1, &m_sceneColor.rtv.cpu, FALSE, &m_depth.dsv.cpu);
	m_commandList->ClearRenderTargetView(m_sceneColor.rtv.cpu, m_sceneColor.ClearColor(), 0, nullptr);
	m_commandList->ClearDepthStencilView(m_depth.dsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	m_commandList->SetGraphicsRootDescriptorTable(3, m_envMap.srv.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(4, m_irradianceMap.srv.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(5, m_specularMap.srv.gpu);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// ── 스카이박스 ──
	m_commandList->SetPipelineState(m_cubeMapPipelineState.Get());
	m_commandList->SetGraphicsRootDescriptorTable(0, m_skyboxVsCbvTable.gpu);
	D3D12_VERTEX_BUFFER_VIEW skyVbv = m_skyboxVB.VertexView();
	D3D12_INDEX_BUFFER_VIEW  skyIbv = m_skyboxIB.IndexView();
	m_commandList->SetGraphicsRootDescriptorTable(2, m_envMap.srv.gpu);
	m_commandList->IASetVertexBuffers(0, 1, &skyVbv);
	m_commandList->IASetIndexBuffer(&skyIbv);
	m_commandList->DrawIndexedInstanced(m_skyboxIB.Count(), 1, 0, 0, 0);
	
	// ── 모델 ──
	m_commandList->SetPipelineState(m_pipelineState.Get());
	m_commandList->SetGraphicsRootDescriptorTable(0, m_vsCbvTable.gpu);   // 원래 VS 상수 버퍼(0,1번 칸)로 복귀
	for (const Mesh& mesh : m_meshes) {
		const Texture& albedo = mesh.albedo.IsValid() ? mesh.albedo : m_defaultTexture;
		D3D12_VERTEX_BUFFER_VIEW vbv = mesh.vb.VertexView();
		D3D12_INDEX_BUFFER_VIEW  ibv = mesh.ib.IndexView();
		m_commandList->SetGraphicsRootDescriptorTable(2, albedo.srv.gpu);
		m_commandList->IASetVertexBuffers(0, 1, &vbv);
		m_commandList->IASetIndexBuffer(&ibv);
		m_commandList->DrawIndexedInstanced(mesh.ib.Count(), 1, 0, 0, 0);
	}

	// 씬 텍스처: RT → SRV
	m_sceneColor.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// ── 후처리 공통 설정 ──
	m_commandList->SetGraphicsRootSignature(m_postRootSignature.Get());   // ⚠ 이전 루트 파라미터는 모두 무효
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CD3DX12_VIEWPORT bloomVp(0.0f, 0.0f, float(m_bloomWidth), float(m_bloomHeight));
	CD3DX12_RECT bloomRect(0, 0, LONG(m_bloomWidth), LONG(m_bloomHeight));
	m_commandList->RSSetViewports(1, &bloomVp);
	m_commandList->RSSetScissorRects(1, &bloomRect);

	// 블룸 패스 하나: target에 src를 읽어서 그림
	auto bloomPass = [&](ID3D12PipelineState* pso, Texture& target, const Texture& src, const PostConstants& c)
		{
			target.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_commandList->OMSetRenderTargets(1, &target.rtv.cpu, FALSE, nullptr);
			m_commandList->SetPipelineState(pso);
			m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(PostConstants) / 4, &c, 0);
			m_commandList->SetGraphicsRootDescriptorTable(1, src.srv.gpu);
			m_commandList->DrawInstanced(3, 1, 0, 0);
			target.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		};

	PostConstants pc = {};
	pc.invSize[0] = 1.0f / m_bloomWidth;
	pc.invSize[1] = 1.0f / m_bloomHeight;
	pc.threshold = m_bloomThreshold;
	pc.strength = m_bloomStrength;
	pc.exposure = m_exposure;

	bloomPass(m_brightPSO.Get(), m_bloom[0], m_sceneColor, pc);
	for (int i = 0; i < m_bloomBlurCount; ++i)
	{
		pc.direction[0] = 1.0f; pc.direction[1] = 0.0f;
		bloomPass(m_blurPSO.Get(), m_bloom[1], m_bloom[0], pc);
		pc.direction[0] = 0.0f; pc.direction[1] = 1.0f;
		bloomPass(m_blurPSO.Get(), m_bloom[0], m_bloom[1], pc);
	}

	// 3) Combine: scene + bloom[0] → 백버퍼
	GpuResource& backBuffer = m_swapChain.CurrentBackBuffer();
	backBuffer.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.CurrentRtv();

	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &m_depth.dsv.cpu);   // ImGui PSO의 DSV 포맷과 맞춤
	m_commandList->ClearRenderTargetView(rtv, kSceneClear, 0, nullptr);   // GUI 영역 배경

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	pc.invSize[0] = 1.0f / m_width;
	pc.invSize[1] = 1.0f / m_height;
	m_commandList->SetPipelineState(m_combinePSO.Get());
	m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(PostConstants) / 4, &pc, 0);
	m_commandList->SetGraphicsRootDescriptorTable(1, m_sceneColor.srv.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(2, m_bloom[0].srv.gpu);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// ImGui: 박스 다음, Present 배리어 전
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	backBuffer.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_PRESENT);

	ThrowIfFailed(m_commandList->Close());
}

void MyEngine::WaitForPreviousFrame()
{
	m_gfx.Flush();
	m_frameIndex = m_swapChain.CurrentIndex();
}

void MyEngine::UpdateGUI() 
{
	ImGui::Checkbox("VSync", &m_vsync);
	if (!m_swapChain.IsTearingSupported())
		ImGui::TextDisabled("(tearing not supported)");

	ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
	ImGui::SliderFloat("Bloom Threshold", &m_bloomThreshold, 0.0f, 3.0f);
	ImGui::SliderFloat("Bloom Strength", &m_bloomStrength, 0.0f, 3.0f);
	ImGui::SliderInt("Bloom Blur Count", &m_bloomBlurCount, 0, 8);

	ImGui::SliderFloat("Rim Strength",
		&m_BasicPixelConstantBufferData.rimStrength, 0.0f,
		10.0f);
	bool useSmoothstep = m_BasicPixelConstantBufferData.useSmoothstep != 0;
	if (ImGui::Checkbox("Use Smoothstep", &useSmoothstep))
		m_BasicPixelConstantBufferData.useSmoothstep = useSmoothstep ? 1u : 0u;
	ImGui::SliderFloat3("Rim Color", &m_BasicPixelConstantBufferData.rimColor.x,
		0.0f, 1.0f);
	ImGui::SliderFloat("Rim Power", &m_BasicPixelConstantBufferData.rimPower,
		0.01f, 10.0f);

	bool useTexture = m_BasicPixelConstantBufferData.useTexture != 0;
	if (ImGui::Checkbox("Use Texture", &useTexture))
		m_BasicPixelConstantBufferData.useTexture = useTexture ? 1u : 0u;
	ImGui::SliderFloat3("m_modelTranslation", &m_modelTranslation.x, -2.0f,
		2.0f);
	ImGui::SliderFloat3("m_modelRotation", &m_modelRotation.x, -3.14f, 3.14f);
	ImGui::SliderFloat3("m_modelScaling", &m_modelScaling.x, 0.1f, 2.0f);
	ImGui::SliderFloat("m_viewRot", &m_viewRot, -3.14f, 3.14f);

	ImGui::SliderFloat3("Material FresnelR0",
		&m_BasicPixelConstantBufferData.material.fresnelR0.x,
		0.0f, 1.0f);
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
	if (!m_swapChain.IsValid() || minimized || width == 0 || height == 0)
		return;

	// 1. GPU가 현재 백버퍼를 다 쓸 때까지 대기
	WaitForPreviousFrame();
	m_swapChain.Resize(width, height);;
	m_frameIndex = m_swapChain.CurrentIndex();

	UpdateForSizeChange(width, height); 
	UpdateSceneViewport();

	m_depth.CreateDepth(m_device.Get(), m_dsvHeap, m_width, m_height, DXGI_FORMAT_D32_FLOAT, L"SceneDepth");
	CreatePostTargets(width, height);
}