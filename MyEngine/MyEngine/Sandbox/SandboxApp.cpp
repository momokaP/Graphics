#include "stdafx.h"
#include "Sandbox/SandboxApp.h"

#include <filesystem>
#include <directxtk12/ResourceUploadBatch.h>

#include "GeometryGenerator.h"
#include "Paths.h"

SandboxApp::SandboxApp(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name)
{
	m_viewport    = CD3DX12_VIEWPORT(0.0f, 0.0f, float(width), float(height));
	m_scissorRect = CD3DX12_RECT(0, 0, LONG(width), LONG(height));
	m_sceneAspect = float(width) / float(height);
}

// ════════════════════════════ 초기화 / 종료 ════════════════════════════

void SandboxApp::OnInit()
{
	Paths::Init();

	Renderer::Desc desc;
	desc.hwnd    = Win32Application::GetHwnd();
	desc.width   = m_width;
	desc.height  = m_height;
	desc.useWarp = m_useWarpDevice;
	m_renderer.Init(desc);           // 디바이스, 힙, 스왑체인, 루트 시그니처, 패스, 렌더 타깃

	LoadAssets();
	InitGUI();
}

void SandboxApp::LoadAssets()
{
	ID3D12Device* device = m_renderer.Device();

	m_renderer.UploadResources([&](DirectX::ResourceUploadBatch& upload)
		{
			// 기본 텍스처
			m_defaultTexture.LoadWIC(device, upload, Paths::Asset(L"Textures/kotone.png"), true);

			// 모델
			MeshData meshData = GeometryGenerator::MakeBox();
			std::vector<MeshData> meshDatas;
			meshDatas.push_back(meshData); // = GeometryGenerator::ReadFromFile(Paths::Asset(L"Models/zaku2/zaku2_1.fbx"));
			m_meshes.resize(meshDatas.size());     // 크기를 미리 확정해 재할당 방지
			for (size_t i = 0; i < meshDatas.size(); ++i)
			{
				const MeshData& md = meshDatas[i];
				Mesh& mesh = m_meshes[i];
				mesh.vb.CreateVertexBuffer(device, upload, md.vertices, L"Mesh VB");
				mesh.ib.CreateIndexBuffer(device, upload, md.indices, L"Mesh IB");
				if (!md.albedoTexturePath.empty() && std::filesystem::exists(md.albedoTexturePath))
					mesh.albedo.LoadWIC(device, upload, md.albedoTexturePath, true);
			}

			// IBL 큐브맵
			const auto ibl = Paths::Asset(L"DayEnvironmentHDRI057Brdf");
			m_envMap.LoadDDS(device, upload, ibl / L"DayEnvironmentHDRI057EnvHDR.dds");
			m_irradianceMap.LoadDDS(device, upload, ibl / L"DayEnvironmentHDRI057DiffuseHDR.dds");
			m_specularMap.LoadDDS(device, upload, ibl / L"DayEnvironmentHDRI057SpecularHDR.dds");
		});

	// SRV는 업로드가 끝난 뒤 (UploadResources가 반환하면 끝난 상태)
	DescriptorHeap& srvHeap = m_renderer.SrvHeap();
	m_defaultTexture.CreateSrv(device, srvHeap);
	for (Mesh& mesh : m_meshes)
		if (mesh.albedo.IsValid()) mesh.albedo.CreateSrv(device, srvHeap);
	m_envMap.CreateSrv(device, srvHeap);
	m_irradianceMap.CreateSrv(device, srvHeap);
	m_specularMap.CreateSrv(device, srvHeap);
}

void SandboxApp::InitGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());   // 플랫폼(창) 쪽은 앱
	m_renderer.InitUI();                                 // DX12 쪽은 Renderer(UIPass)
}

void SandboxApp::OnDestroy()
{
	m_renderer.Shutdown();           // GPU 대기 + DX12 UI 백엔드 + 디바이스 정리
	ImGui_ImplWin32_Shutdown();      // 그다음 플랫폼 쪽
	ImGui::DestroyContext();
}

// ════════════════════════════ 프레임 ════════════════════════════

void SandboxApp::OnUpdate(float dt)
{
	using namespace DirectX;

	// ── 프레임 상수: 카메라 ──
	const Matrix view = Matrix::CreateRotationY(m_viewRot) * Matrix::CreateTranslation(0.0f, 0.0f, 2.0f);
	const Matrix proj = m_usePerspectiveProjection
		? Matrix(XMMatrixPerspectiveFovLH(XMConvertToRadians(m_projFovAngleY), m_sceneAspect, m_nearZ, m_farZ))
		: Matrix(XMMatrixOrthographicOffCenterLH(-m_sceneAspect, m_sceneAspect, -1.0f, 1.0f, m_nearZ, m_farZ));

	m_frameConstants.view     = view;
	m_frameConstants.proj     = proj;
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
	m_material.diffuse  = Vector3(m_materialDiffuse);
	m_material.specular = Vector3(m_materialSpecular);

	BuildRenderScene();
}

void SandboxApp::BuildRenderScene()
{
	m_renderScene.frame    = m_frameConstants;
	m_renderScene.material = m_material;

	m_renderScene.draws.clear();     // capacity는 유지 → 매 프레임 재할당 없음
	for (const Mesh& mesh : m_meshes)
	{
		DrawItem d;
		d.vertexBuffer = &mesh.vb;
		d.indexBuffer  = &mesh.ib;
		d.albedo       = mesh.albedo.IsValid() ? &mesh.albedo : &m_defaultTexture;
		d.object       = m_modelObject;
		m_renderScene.draws.push_back(d);
	}

	m_renderScene.envMap        = &m_envMap;
	m_renderScene.irradianceMap = &m_irradianceMap;
	m_renderScene.specularMap   = &m_specularMap;

	m_renderScene.post.exposure       = m_exposure;
	m_renderScene.post.bloomThreshold = m_bloomThreshold;
	m_renderScene.post.bloomStrength  = m_bloomStrength;
	m_renderScene.post.bloomBlurCount = m_bloomBlurCount;

	m_renderScene.viewport = m_viewport;
	m_renderScene.scissor  = m_scissorRect;
}

void SandboxApp::OnRender()
{
	m_renderer.Render(m_renderScene);
}

// ════════════════════════════ 창 / GUI ════════════════════════════

void SandboxApp::OnSizeChanged(UINT width, UINT height, bool minimized)
{
	if (!m_renderer.IsInitialized() || minimized || width == 0 || height == 0)
		return;

	m_renderer.Resize(width, height);     // GPU 대기 + 스왑체인 + 렌더 타깃
	UpdateForSizeChange(width, height);   // m_width, m_height
	UpdateSceneViewport();
}

void SandboxApp::UpdateSceneViewport()
{
	float sceneWidth = float(m_width) - float(m_guiWidth);
	if (sceneWidth < 1.0f || m_height == 0)
		return;   // GUI가 창 전체를 덮거나 최소화된 경우

	m_viewport    = CD3DX12_VIEWPORT(float(m_guiWidth), 0.0f, sceneWidth, float(m_height));
	m_scissorRect = CD3DX12_RECT(m_guiWidth, 0, LONG(m_width), LONG(m_height));
	m_sceneAspect = sceneWidth / float(m_height);
}

void SandboxApp::UpdateGUI()
{
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SandboxApp (Renderer)");   // 어느 버전인지 표시
	ImGui::Text("CPU wait for GPU: %.2f ms", m_renderer.LastGpuWaitMs());
	ImGui::Text("Upload: %.1f KB", m_renderer.UploadBytesUsed() / 1024.0);

	bool vsync = m_renderer.VSync();
	if (ImGui::Checkbox("VSync", &vsync))
		m_renderer.SetVSync(vsync);
	if (!m_renderer.IsTearingSupported())
		ImGui::TextDisabled("(tearing not supported)");

	ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 5.0f);
	ImGui::SliderFloat("Bloom Threshold", &m_bloomThreshold, 0.0f, 3.0f);
	ImGui::SliderFloat("Bloom Strength", &m_bloomStrength, 0.0f, 3.0f);
	ImGui::SliderInt("Bloom Blur Count", &m_bloomBlurCount, 0, 8);

	ImGui::SliderFloat("Rim Strength", &m_material.rimStrength, 0.0f, 10.0f);
	bool useSmoothstep = m_material.useSmoothstep != 0;
	if (ImGui::Checkbox("Use Smoothstep", &useSmoothstep))
		m_material.useSmoothstep = useSmoothstep ? 1u : 0u;
	ImGui::SliderFloat3("Rim Color", &m_material.rimColor.x, 0.0f, 1.0f);
	ImGui::SliderFloat("Rim Power", &m_material.rimPower, 0.01f, 10.0f);

	bool useTexture = m_material.useTexture != 0;
	if (ImGui::Checkbox("Use Texture", &useTexture))
		m_material.useTexture = useTexture ? 1u : 0u;
	ImGui::SliderFloat3("m_modelTranslation", &m_modelTranslation.x, -2.0f, 2.0f);
	ImGui::SliderFloat3("m_modelRotation", &m_modelRotation.x, -3.14f, 3.14f);
	ImGui::SliderFloat3("m_modelScaling", &m_modelScaling.x, 0.1f, 2.0f);
	ImGui::SliderFloat("m_viewRot", &m_viewRot, -3.14f, 3.14f);

	ImGui::SliderFloat3("Material FresnelR0", &m_material.fresnelR0.x, 0.0f, 1.0f);
	ImGui::SliderFloat("Material Shininess", &m_material.shininess, 1.0f, 256.0f);

	if (ImGui::RadioButton("Directional Light", m_lightType == 0)) m_lightType = 0;
	ImGui::SameLine();
	if (ImGui::RadioButton("Point Light", m_lightType == 1)) m_lightType = 1;
	ImGui::SameLine();
	if (ImGui::RadioButton("Spot Light", m_lightType == 2)) m_lightType = 2;

	ImGui::SliderFloat("Material Diffuse", &m_materialDiffuse, 0.0f, 1.0f);
	ImGui::SliderFloat("Material Specular", &m_materialSpecular, 0.0f, 1.0f);

	ImGui::SliderFloat3("Light Strength", &m_lightFromGUI.strength.x, 0.0f, 5.0f);
	ImGui::SliderFloat3("Light Position", &m_lightFromGUI.position.x, -5.0f, 5.0f);
	ImGui::SliderFloat("Light fallOffStart", &m_lightFromGUI.fallOffStart, 0.0f, 5.0f);
	ImGui::SliderFloat("Light fallOffEnd", &m_lightFromGUI.fallOffEnd, 0.0f, 10.0f);
	ImGui::SliderFloat("Light spotPower", &m_lightFromGUI.spotPower, 1.0f, 512.0f);

	int newWidth = int(ImGui::GetWindowWidth());
	if (newWidth != m_guiWidth)
	{
		m_guiWidth = newWidth;
		UpdateSceneViewport();   // OnUpdate 전에 aspect까지 확정
	}
}
