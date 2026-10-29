#include "stdafx.h"
#include "MyEngine.h"
#include <filesystem>
#include <chrono>

namespace
{
	hlsl::ObjectConstants MakeObjectConstants(const Matrix& world)
	{
		hlsl::ObjectConstants o;
		o.world = world;
		// 노멀은 w=0이라 평행이동은 영향 없음 → 역행렬의 전치만 하면 됨 (비균일 스케일에서도 노멀이 올바름)
		o.worldInvTranspose = world.Invert().Transpose();
		return o;
	}
}

MyEngine::MyEngine(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
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
	m_gfx.Device();
	m_gfx.Queue();

	m_rtvHeap.Init(m_gfx.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16, false, L"RTV Heap");
	m_dsvHeap.Init(m_gfx.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 4, false, L"DSV Heap");
	m_srvHeap.Init(m_gfx.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true, L"CBV/SRV Heap");

	SwapChain::Desc scDesc;
	scDesc.width = m_width;
	scDesc.height = m_height;
	scDesc.bufferCount = FrameCount;
	scDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_swapChain.Init(m_gfx, m_rtvHeap, Win32Application::GetHwnd(), scDesc);

	m_depth.CreateDepth(m_gfx.Device(), m_dsvHeap, m_width, m_height, DXGI_FORMAT_D32_FLOAT, L"SceneDepth");

	for (UINT i = 0; i < FrameCount; ++i)
	{
		ThrowIfFailed(m_gfx.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&m_frames[i].allocator)));
		wchar_t name[32];
		swprintf_s(name, L"CommandAllocator[%u]", i);
		m_frames[i].allocator->SetName(name);
	}
}

static const float kSceneClear[4] = { 0.2f, 0.2f, 0.25f, 1.0f };

void MyEngine::CreatePostTargets(UINT width, UINT height)
{
	static const float kBlack[4] = { 0, 0, 0, 1 };
	m_sceneColor.CreateRenderTarget(m_gfx.Device(), m_rtvHeap, m_srvHeap,
		width, height, kHdrFormat, kSceneClear, L"SceneColor");

	m_bloomWidth = max(1u, width / 2);
	m_bloomHeight = max(1u, height / 2);
	m_bloom[0].CreateRenderTarget(m_gfx.Device(), m_rtvHeap, m_srvHeap,
		m_bloomWidth, m_bloomHeight, kHdrFormat, kBlack, L"Bloom[0]");
	m_bloom[1].CreateRenderTarget(m_gfx.Device(), m_rtvHeap, m_srvHeap,
		m_bloomWidth, m_bloomHeight, kHdrFormat, kBlack, L"Bloom[1]");
}

void MyEngine::LoadAssets()
{
	{
		using namespace StaticSamplers;
		constexpr auto PS = D3D12_SHADER_VISIBILITY_PIXEL;
		constexpr auto SRV = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		constexpr auto VOLATILE = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;   // 같은 프레임에 쓰였다가 읽히는 텍스처
		constexpr auto STATIC = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;     // 로드 후 절대 안 바뀜

		RootSignatureBuilder rs;
		rs.CBV(0, D3D12_SHADER_VISIBILITY_ALL)            // RP_Frame
			.CBV(1, D3D12_SHADER_VISIBILITY_VERTEX)         // RP_Object
			.CBV(2, PS)                                     // RP_Material
			.Constants(kPostConstantCount, 3, PS)           // RP_PassConstants
			.Table(SRV, 1, 0, PS, VOLATILE)                 // RP_Tex0
			.Table(SRV, 1, 1, PS, VOLATILE)                 // RP_Tex1
			.Table(SRV, 1, 2, PS, VOLATILE)                 // RP_Tex2
			.Table(SRV, 1, 3, PS, VOLATILE)                 // RP_Tex3
			.Table(SRV, 1, 4, PS, STATIC)                   // RP_EnvMap
			.Table(SRV, 1, 5, PS, STATIC)                   // RP_Irradiance
			.Table(SRV, 1, 6, PS, STATIC)                   // RP_Specular
			.StaticSampler(PointClamp(0))
			.StaticSampler(LinearClamp(1))
			.StaticSampler(LinearWrap(2))
			.StaticSampler(AnisoWrap(3))
			.Flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
		assert(rs.ParamCount() == RP_Count);
		m_rootSignature = rs.Build(m_gfx.Device(), L"Engine RootSig");
	}

	{
		auto vs = LoadShader(Paths::Shader(L"VertexShader.cso"));
		auto ps = LoadShader(Paths::Shader(L"PixelShader.cso"));
		auto skyPs = LoadShader(Paths::Shader(L"CubeMappingPixelShader.cso"));

		m_pipelineState = PipelineBuilder(m_rootSignature.Get())
			.VS(vs).PS(ps).InputLayout(kVertexLayout)
			.RenderTarget(kHdrFormat).DepthFormat(DXGI_FORMAT_D32_FLOAT)
			.Cull(D3D12_CULL_MODE_NONE)
			.Build(m_gfx.Device(), L"Forward PSO");

		m_cubeMapPipelineState = PipelineBuilder(m_rootSignature.Get())
			.VS(vs).PS(skyPs).InputLayout(kVertexLayout)
			.RenderTarget(kHdrFormat).DepthFormat(DXGI_FORMAT_D32_FLOAT)
			.Cull(D3D12_CULL_MODE_NONE).DepthWrite(false)        // ← 두 PSO의 유일한 차이가 한눈에 보임
			.Build(m_gfx.Device(), L"Skybox PSO");
	}

	{
		auto fullscreenVs = LoadShader(Paths::Shader(L"FullscreenVS.cso"));
		auto makePost = [&](const wchar_t* psFile, DXGI_FORMAT rtv, DXGI_FORMAT dsv, const wchar_t* name)
			{
				auto ps = LoadShader(Paths::Shader(psFile));      // Build까지 같은 식 안에서 끝나므로 수명 OK
				return PipelineBuilder(m_rootSignature.Get())
					.VS(fullscreenVs).PS(ps)                       // 입력 레이아웃 없음 (SV_VertexID로 삼각형 생성)
					.RenderTarget(rtv).DepthFormat(dsv)
					.Cull(D3D12_CULL_MODE_NONE).DepthTest(false)
					.Build(m_gfx.Device(), name);
			};
		m_brightPSO = makePost(L"BloomBrightPS.cso", kHdrFormat, DXGI_FORMAT_UNKNOWN, L"Bloom Bright PSO");
		m_blurPSO = makePost(L"BloomBlurPS.cso", kHdrFormat, DXGI_FORMAT_UNKNOWN, L"Bloom Blur PSO");
		m_combinePSO = makePost(L"BloomCombinePS.cso", m_swapChain.Format(), DXGI_FORMAT_D32_FLOAT, L"Combine PSO");
	}

	ThrowIfFailed(m_gfx.Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_frames[0].allocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	m_commandList->SetName(L"Main CommandList");
	ThrowIfFailed(m_commandList->Close());

	CreatePostTargets(m_width, m_height);

	{
		DirectX::ResourceUploadBatch upload(m_gfx.Device());
		upload.Begin();

		// 스카이박스용 박스
		MeshData box = GeometryGenerator::MakeBox();
		m_skyboxVB.CreateVertexBuffer(m_gfx.Device(), upload, box.vertices, L"Skybox VB");
		m_skyboxIB.CreateIndexBuffer(m_gfx.Device(), upload, box.indices, L"Skybox IB");

		// 기본 텍스처
		m_defaultTexture.LoadWIC(m_gfx.Device(), upload, Paths::Asset(L"Textures/kotone.png"), true);

		// 모델
		auto meshDatas = GeometryGenerator::ReadFromFile(Paths::Asset(L"Models/zaku2/zaku2_1.fbx"));
		m_meshes.resize(meshDatas.size());     // 크기를 미리 확정해 재할당 방지
		for (size_t i = 0; i < meshDatas.size(); ++i)
		{
			const MeshData& md = meshDatas[i];
			Mesh& mesh = m_meshes[i];
			mesh.vb.CreateVertexBuffer(m_gfx.Device(), upload, md.vertices, L"Mesh VB");
			mesh.ib.CreateIndexBuffer(m_gfx.Device(), upload, md.indices, L"Mesh IB");
			if (!md.albedoTexturePath.empty() && std::filesystem::exists(md.albedoTexturePath))
				mesh.albedo.LoadWIC(m_gfx.Device(), upload, md.albedoTexturePath, true);
		}

		// IBL 큐브맵
		const auto ibl = Paths::Asset(L"DayEnvironmentHDRI057Brdf");
		m_envMap.LoadDDS(m_gfx.Device(), upload, ibl / L"DayEnvironmentHDRI057EnvHDR.dds");
		m_irradianceMap.LoadDDS(m_gfx.Device(), upload, ibl / L"DayEnvironmentHDRI057DiffuseHDR.dds");
		m_specularMap.LoadDDS(m_gfx.Device(), upload, ibl / L"DayEnvironmentHDRI057SpecularHDR.dds");

		upload.End(m_gfx.Queue()).wait();

		// SRV는 업로드가 끝난 뒤 생성
		m_defaultTexture.CreateSrv(m_gfx.Device(), m_srvHeap);
		for (Mesh& mesh : m_meshes)
			if (mesh.albedo.IsValid()) mesh.albedo.CreateSrv(m_gfx.Device(), m_srvHeap);
		m_envMap.CreateSrv(m_gfx.Device(), m_srvHeap);
		m_irradianceMap.CreateSrv(m_gfx.Device(), m_srvHeap);
		m_specularMap.CreateSrv(m_gfx.Device(), m_srvHeap);
	}

	{
		// 동적 상수: 프레임당 1MB (256바이트 상수 4096개분)
		m_uploadAlloc.Init(m_gfx.Device(), FrameCount, 1 * 1024 * 1024, L"Dynamic Upload");
	}

	{
		WaitForGpuIdle();
	}
}

void MyEngine::InitGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(Win32Application::GetHwnd());

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = m_gfx.Device();
	initInfo.CommandQueue = m_gfx.Queue();
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

	// ── 프레임 상수: 카메라 ──
	const Matrix view = Matrix::CreateRotationY(m_viewRot) * Matrix::CreateTranslation(0.0f, 0.0f, 2.0f);
	const Matrix proj = m_usePerspectiveProjection
		? Matrix(XMMatrixPerspectiveFovLH(XMConvertToRadians(m_projFovAngleY), m_sceneAspect, m_nearZ, m_farZ))
		: Matrix(XMMatrixOrthographicOffCenterLH(-m_sceneAspect, m_sceneAspect, -1.0f, 1.0f, m_nearZ, m_farZ));

	m_frameConstants.view = view;
	m_frameConstants.proj = proj;
	m_frameConstants.viewProj = view * proj;                               // 행벡터 규약: v * V * P
	m_frameConstants.eyeWorld = Vector3::Transform(Vector3::Zero, view.Invert());

	// ── 프레임 상수: 조명 (선택한 1개만 켬) ──
	for (int i = 0; i < MAX_LIGHTS; ++i)
		m_frameConstants.lights[i] = (i == m_lightType) ? m_lightFromGUI : hlsl::Light{};   // 꺼진 조명은 0

	// ── 오브젝트 상수 ──
	const Matrix world =
		Matrix::CreateScale(m_modelScaling) *
		Matrix::CreateRotationY(m_modelRotation.y) *
		Matrix::CreateRotationX(m_modelRotation.x) *
		Matrix::CreateRotationZ(m_modelRotation.z) *
		Matrix::CreateTranslation(m_modelTranslation);
	m_modelObject = MakeObjectConstants(world);
	m_skyboxObject = MakeObjectConstants(Matrix::CreateScale(20.0f));

	// ── 머티리얼 상수 ──
	m_material.diffuse = Vector3(m_materialDiffuse);
	m_material.specular = Vector3(m_materialSpecular);
}

void MyEngine::OnRender()
{
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_gfx.Queue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_swapChain.Present(m_vsync);

	MoveToNextFrame();
}

void MyEngine::OnDestroy()
{
	WaitForGpuIdle();
	m_releaseQueue.Flush();
	
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_gfx.Shutdown();
}

void MyEngine::PopulateCommandList()
{
	FrameContext& frame = m_frames[m_frameIndex];

	// 이 할당자를 쓴 이전 프레임은 MoveToNextFrame에서 이미 끝났음이 보장됨
	ThrowIfFailed(frame.allocator->Reset());
	ThrowIfFailed(m_commandList->Reset(frame.allocator.Get(), m_pipelineState.Get()));

	m_uploadAlloc.BeginFrame(m_frameIndex);      // ← 이번 프레임 영역을 처음부터 사용

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Heap()};
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->SetGraphicsRootConstantBufferView(RP_Frame, m_uploadAlloc.Push(m_frameConstants));
	m_commandList->SetGraphicsRootConstantBufferView(RP_Material, m_uploadAlloc.Push(m_material));
	m_commandList->SetGraphicsRootDescriptorTable(RP_EnvMap, m_envMap.srv.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(RP_Irradiance, m_irradianceMap.srv.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(RP_Specular, m_specularMap.srv.gpu);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->RSSetViewports(1, &m_viewport);
	
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	m_sceneColor.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->OMSetRenderTargets(1, &m_sceneColor.rtv.cpu, FALSE, &m_depth.dsv.cpu);
	m_commandList->ClearRenderTargetView(m_sceneColor.rtv.cpu, m_sceneColor.ClearColor(), 0, nullptr);
	m_commandList->ClearDepthStencilView(m_depth.dsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// ── 스카이박스 ──
	m_commandList->SetPipelineState(m_cubeMapPipelineState.Get());
	m_commandList->SetGraphicsRootConstantBufferView(RP_Object, m_uploadAlloc.Push(m_skyboxObject));
	D3D12_VERTEX_BUFFER_VIEW skyVbv = m_skyboxVB.VertexView();
	D3D12_INDEX_BUFFER_VIEW  skyIbv = m_skyboxIB.IndexView();
	m_commandList->IASetVertexBuffers(0, 1, &skyVbv);
	m_commandList->IASetIndexBuffer(&skyIbv);
	m_commandList->DrawIndexedInstanced(m_skyboxIB.Count(), 1, 0, 0, 0);
	
	// ── 모델 ──
	m_commandList->SetPipelineState(m_pipelineState.Get());
	m_commandList->SetGraphicsRootConstantBufferView(RP_Object, m_uploadAlloc.Push(m_modelObject));

	for (const Mesh& mesh : m_meshes) {
		const Texture& albedo = mesh.albedo.IsValid() ? mesh.albedo : m_defaultTexture;
		D3D12_VERTEX_BUFFER_VIEW vbv = mesh.vb.VertexView();
		D3D12_INDEX_BUFFER_VIEW  ibv = mesh.ib.IndexView();
		m_commandList->SetGraphicsRootDescriptorTable(RP_Tex0, albedo.srv.gpu);
		m_commandList->IASetVertexBuffers(0, 1, &vbv);
		m_commandList->IASetIndexBuffer(&ibv);
		m_commandList->DrawIndexedInstanced(mesh.ib.Count(), 1, 0, 0, 0);
	}

	// 씬 텍스처: RT → SRV
	m_sceneColor.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// ── 후처리 공통 설정 ──
	CD3DX12_VIEWPORT bloomVp(0.0f, 0.0f, float(m_bloomWidth), float(m_bloomHeight));
	CD3DX12_RECT bloomRect(0, 0, LONG(m_bloomWidth), LONG(m_bloomHeight));
	m_commandList->RSSetViewports(1, &bloomVp);
	m_commandList->RSSetScissorRects(1, &bloomRect);

	// 블룸 패스 하나: target에 src를 읽어서 그림
	auto bloomPass = [&](ID3D12PipelineState* pso, Texture& target, const Texture& src, const hlsl::PostConstants& c)
		{
			target.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_commandList->OMSetRenderTargets(1, &target.rtv.cpu, FALSE, nullptr);
			m_commandList->SetPipelineState(pso);
			m_commandList->SetGraphicsRoot32BitConstants(RP_PassConstants, kPostConstantCount, &c, 0);
			m_commandList->SetGraphicsRootDescriptorTable(RP_Tex0, src.srv.gpu);
			m_commandList->DrawInstanced(3, 1, 0, 0);
			target.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		};

	hlsl::PostConstants pc{};
	pc.invSize = Vector2(1.0f / m_bloomWidth, 1.0f / m_bloomHeight);
	pc.threshold = m_bloomThreshold;
	pc.strength = m_bloomStrength;
	pc.exposure = m_exposure;

	bloomPass(m_brightPSO.Get(), m_bloom[0], m_sceneColor, pc);
	for (int i = 0; i < m_bloomBlurCount; ++i)
	{
		pc.direction = Vector2(1.0f, 0.0f);
		bloomPass(m_blurPSO.Get(), m_bloom[1], m_bloom[0], pc);
		pc.direction = Vector2(0.0f, 1.0f);
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

	pc.invSize = Vector2(1.0f / m_width, 1.0f / m_height);
	m_commandList->SetPipelineState(m_combinePSO.Get());
	m_commandList->SetGraphicsRoot32BitConstants(RP_PassConstants, kPostConstantCount, &pc, 0);
	m_commandList->SetGraphicsRootDescriptorTable(RP_Tex0, m_sceneColor.srv.gpu);
	m_commandList->SetGraphicsRootDescriptorTable(RP_Tex1, m_bloom[0].srv.gpu);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// ImGui: 박스 다음, Present 배리어 전
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

	backBuffer.Transition(m_commandList.Get(), D3D12_RESOURCE_STATE_PRESENT);

	ThrowIfFailed(m_commandList->Close());
}

void MyEngine::MoveToNextFrame()
{
	// ① 방금 제출한 프레임에 "여기까지 끝나면 이 값" 표시를 큐에 넣음
	m_frames[m_frameIndex].fenceValue = m_gfx.Signal();

	// ② 다음 프레임 슬롯으로 이동
	m_frameIndex = (m_frameIndex + 1) % FrameCount;

	// ③ 그 슬롯을 마지막으로 쓴 프레임(= FrameCount 프레임 전)이 GPU에서 끝날 때까지만 대기
	//    CPU가 GPU보다 최대 FrameCount 프레임까지만 앞서 가도록 제한하는 지점
	const auto t0 = std::chrono::high_resolution_clock::now();
	m_gfx.WaitForFence(m_frames[m_frameIndex].fenceValue);
	const auto t1 = std::chrono::high_resolution_clock::now();
	m_lastGpuWaitMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
	
	m_releaseQueue.Collect(m_gfx.CompletedValue());
}

void MyEngine::WaitForGpuIdle()
{
	m_gfx.Flush();   // GPU가 받은 모든 작업이 끝날 때까지 대기
}

void MyEngine::UpdateGUI() 
{
	ImGui::Text("CPU wait for GPU: %.2f ms", m_lastGpuWaitMs);

	ImGui::Checkbox("VSync", &m_vsync);
	if (!m_swapChain.IsTearingSupported())
		ImGui::TextDisabled("(tearing not supported)");

	ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
	ImGui::SliderFloat("Bloom Threshold", &m_bloomThreshold, 0.0f, 3.0f);
	ImGui::SliderFloat("Bloom Strength", &m_bloomStrength, 0.0f, 3.0f);
	ImGui::SliderInt("Bloom Blur Count", &m_bloomBlurCount, 0, 8);

	ImGui::SliderFloat("Rim Strength",
		&m_material.rimStrength, 0.0f,
		10.0f);
	bool useSmoothstep = m_material.useSmoothstep != 0;
	if (ImGui::Checkbox("Use Smoothstep", &useSmoothstep))
		m_material.useSmoothstep = useSmoothstep ? 1u : 0u;
	ImGui::SliderFloat3("Rim Color", &m_material.rimColor.x,
		0.0f, 1.0f);
	ImGui::SliderFloat("Rim Power", &m_material.rimPower,
		0.01f, 10.0f);

	bool useTexture = m_material.useTexture != 0;
	if (ImGui::Checkbox("Use Texture", &useTexture))
		m_material.useTexture = useTexture ? 1u : 0u;
	ImGui::SliderFloat3("m_modelTranslation", &m_modelTranslation.x, -2.0f,
		2.0f);
	ImGui::SliderFloat3("m_modelRotation", &m_modelRotation.x, -3.14f, 3.14f);
	ImGui::SliderFloat3("m_modelScaling", &m_modelScaling.x, 0.1f, 2.0f);
	ImGui::SliderFloat("m_viewRot", &m_viewRot, -3.14f, 3.14f);

	ImGui::SliderFloat3("Material FresnelR0",
		&m_material.fresnelR0.x,
		0.0f, 1.0f);
	ImGui::SliderFloat("Material Shininess",
		&m_material.shininess, 1.0f,
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
	WaitForGpuIdle();                   // 진행 중인 2프레임 모두 끝날 때까지
	m_swapChain.Resize(width, height);

	UpdateForSizeChange(width, height); 
	UpdateSceneViewport();

	m_depth.CreateDepth(m_gfx.Device(), m_dsvHeap, m_width, m_height, DXGI_FORMAT_D32_FLOAT, L"SceneDepth");
	CreatePostTargets(width, height);
}