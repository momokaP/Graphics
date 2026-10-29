#include "stdafx.h"
#include "MyEngine.h"
#include <filesystem>
#include <chrono>

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

	ThrowIfFailed(m_gfx.Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_frames[0].allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	m_commandList->SetName(L"Main CommandList");
	ThrowIfFailed(m_commandList->Close());

	{
		DirectX::ResourceUploadBatch upload(m_gfx.Device());
		upload.Begin();

		RenderInitContext initCtx = MakeInitContext();
		initCtx.upload = &upload;                   // 패스 Init 동안만 유효
		m_skyboxPass.Init(initCtx);
		m_forwardPass.Init(initCtx);
		m_bloomPass.Init(initCtx);
		m_tonemapPass.Init(initCtx);

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

	CreateSizeDependentTargets(m_width, m_height);

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
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());   // 플랫폼(창) 쪽은 앱에 남김

	m_uiPass.Init(MakeInitContext());                    // DX12 쪽은 UIPass
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
	m_modelObject = hlsl::MakeObjectConstants(world);

	// ── 머티리얼 상수 ──
	m_material.diffuse = Vector3(m_materialDiffuse);
	m_material.specular = Vector3(m_materialSpecular);

	// ── 렌더러에 넘길 데이터 ──
	m_renderScene.frame = m_frameConstants;
	m_renderScene.material = m_material;

	m_renderScene.draws.clear();
	for (const Mesh& mesh : m_meshes)
	{
		DrawItem d;
		d.vertexBuffer = &mesh.vb;
		d.indexBuffer = &mesh.ib;
		d.albedo = mesh.albedo.IsValid() ? &mesh.albedo : &m_defaultTexture;
		d.object = m_modelObject;
		m_renderScene.draws.push_back(d);
	}

	m_renderScene.envMap = &m_envMap;
	m_renderScene.irradianceMap = &m_irradianceMap;
	m_renderScene.specularMap = &m_specularMap;

	m_renderScene.post.exposure = m_exposure;
	m_renderScene.post.bloomThreshold = m_bloomThreshold;
	m_renderScene.post.bloomStrength = m_bloomStrength;
	m_renderScene.post.bloomBlurCount = m_bloomBlurCount;

	m_renderScene.viewport = m_viewport;
	m_renderScene.scissor = m_scissorRect;
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

	m_uiPass.Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_gfx.Shutdown();
}

void MyEngine::PopulateCommandList()
{
	FrameContext& frame = m_frames[m_frameIndex];
	ThrowIfFailed(frame.allocator->Reset());
	ThrowIfFailed(m_commandList->Reset(frame.allocator.Get(), nullptr));
	m_uploadAlloc.BeginFrame(m_frameIndex);

	ID3D12GraphicsCommandList* cmd = m_commandList.Get();
	const RenderScene& scene = m_renderScene;

	RenderTargets targets;
	targets.sceneColor = &m_sceneColor;
	targets.depth = &m_depth;
	targets.backBuffer = &m_swapChain.CurrentBackBuffer();
	targets.backBufferRtv = m_swapChain.CurrentRtv();
	targets.width = m_width;
	targets.height = m_height;

	RenderContext ctx{ cmd, &m_uploadAlloc, &scene, &targets };
	auto run = [&](RenderPass& pass)
		{
			GpuMarker marker(cmd, pass.Name());     // PIX에서 패스 이름으로 묶임
			pass.Execute(ctx);
		};

	// ════ 프레임 공통 바인딩 (모든 패스의 전제) ════
	cmd->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Heap() };
	cmd->SetDescriptorHeaps(_countof(heaps), heaps);
	cmd->SetGraphicsRootConstantBufferView(RP_Frame, m_uploadAlloc.Push(scene.frame));
	cmd->SetGraphicsRootDescriptorTable(RP_EnvMap, scene.envMap->srv.gpu);
	cmd->SetGraphicsRootDescriptorTable(RP_Irradiance, scene.irradianceMap->srv.gpu);
	cmd->SetGraphicsRootDescriptorTable(RP_Specular, scene.specularMap->srv.gpu);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ════ 씬 (HDR) ════
	{
		GpuMarker marker(cmd, L"Scene");
		m_sceneColor.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->OMSetRenderTargets(1, &m_sceneColor.rtv.cpu, FALSE, &m_depth.dsv.cpu);
		cmd->ClearRenderTargetView(m_sceneColor.rtv.cpu, m_sceneColor.ClearColor(), 0, nullptr);
		cmd->ClearDepthStencilView(m_depth.dsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmd->RSSetViewports(1, &scene.viewport);
		cmd->RSSetScissorRects(1, &scene.scissor);

		run(m_skyboxPass);
		run(m_forwardPass);

		m_sceneColor.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// ════ 후처리 ════
	run(m_bloomPass);
	targets.bloom = &m_bloomPass.Output();

	// ════ 백버퍼 (LDR) ════
	targets.backBuffer->Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmd->OMSetRenderTargets(1, &targets.backBufferRtv, FALSE, &m_depth.dsv.cpu);   // DSV는 ImGui PSO 포맷과 맞추기 위함
	cmd->ClearRenderTargetView(targets.backBufferRtv, kSceneClear, 0, nullptr);    // GUI 영역 배경

	run(m_tonemapPass);
	run(m_uiPass);            // ⚠ 항상 마지막 (ImGui가 루트 시그니처를 바꿈)

	targets.backBuffer->Transition(cmd, D3D12_RESOURCE_STATE_PRESENT);
	ThrowIfFailed(cmd->Close());
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

	WaitForGpuIdle();                   // 진행 중인 2프레임 모두 끝날 때까지
	m_swapChain.Resize(width, height);
	UpdateForSizeChange(width, height); 
	UpdateSceneViewport();
	CreateSizeDependentTargets(width, height);
}

RenderInitContext MyEngine::MakeInitContext()
{
	RenderInitContext c;
	c.device = m_gfx.Device();
	c.queue = m_gfx.Queue();
	c.rootSignature = m_rootSignature.Get();
	c.rtvHeap = &m_rtvHeap;
	c.dsvHeap = &m_dsvHeap;
	c.srvHeap = &m_srvHeap;
	c.backBufferFormat = m_swapChain.Format();
	c.frameCount = FrameCount;
	return c;
}

void MyEngine::CreateSizeDependentTargets(UINT width, UINT height)
{
	RenderInitContext ctx = MakeInitContext();
	m_depth.CreateDepth(ctx.device, m_dsvHeap, width, height, kDepthFormat, L"SceneDepth");
	m_sceneColor.CreateRenderTarget(ctx.device, m_rtvHeap, m_srvHeap,
		width, height, kHdrFormat, kSceneClear, L"SceneColor");
	m_bloomPass.Resize(ctx, width, height);
}