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
	m_assets.Init(m_renderer.Device(), &m_renderer.SrvHeap());

	MeshHandle box, sphere, ground;
	m_renderer.UploadResources([&](DirectX::ResourceUploadBatch& up)
		{
			m_assets.SetDefaultTexture(m_assets.LoadTexture(up, Paths::Asset(L"Textures/kotone.png"), true));

			box = m_assets.CreateMesh(up, GeometryGenerator::MakeBox(), L"Box");
			sphere = m_assets.CreateMesh(up, GeometryGenerator::MakeSphere(1.0f, 32, 16), L"Sphere");
			ground = m_assets.CreateMesh(up, GeometryGenerator::MakeSquare(), L"Ground");

			const auto ibl = Paths::Asset(L"DayEnvironmentHDRI057Brdf");
			m_envMap = m_assets.LoadTexture(up, ibl / L"DayEnvironmentHDRI057EnvHDR.dds", false);
			m_irradianceMap = m_assets.LoadTexture(up, ibl / L"DayEnvironmentHDRI057DiffuseHDR.dds", false);
			m_specularMap = m_assets.LoadTexture(up, ibl / L"DayEnvironmentHDRI057SpecularHDR.dds", false);
		});
	m_assets.FinalizeUploads();      // 업로드가 끝난 뒤 SRV 생성

	Transform t;
	t.position = { -0.8f, 0.0f, 0.0f }; t.scale = Vector3(0.4f);
	m_scene.Add("Box", box, t);

	t = {};
	t.position = { 0.8f, 0.0f, 0.0f }; t.scale = Vector3(0.4f);
	m_scene.Add("Sphere", sphere, t);

	t = {};
	t.position = { 0.0f, -0.5f, 0.0f }; t.rotation.x = DirectX::XM_PIDIV2; t.scale = Vector3(3.0f);
	m_scene.Add("Ground", ground, t);
	// MakeSquare가 XY 평면이면 X축 90도로 눕힘. 방향이 반대면 -XM_PIDIV2
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
	CameraInput in;
	const ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureKeyboard)
	{
		in.forward = float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S));
		in.right = float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A));
		in.up = float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q));
		in.fast = io.KeyShift;
	}
	if (!io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Right))
	{
		in.look = true;
		in.mouseDx = io.MouseDelta.x;
		in.mouseDy = io.MouseDelta.y;
	}
	m_scene.camera.SetAspect(m_sceneAspect);
	m_scene.camera.Update(dt, in);
	
	const Matrix view = m_scene.camera.View();
	const Matrix proj = m_scene.camera.Proj(m_usePerspectiveProjection);
	m_frameConstants.view     = view;
	m_frameConstants.proj     = proj;
	m_frameConstants.viewProj = view * proj;                               // 행벡터 규약: v * V * P
	m_frameConstants.eyeWorld = m_scene.camera.Position();

	// ── 프레임 상수: 조명 (선택한 1개만 켬) ──
	for (int i = 0; i < MAX_LIGHTS; ++i)
		m_frameConstants.lights[i] = (i == m_scene.lightType) ? m_scene.light : hlsl::Light{};   // 꺼진 조명은 0

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
	for (const Entity& e : m_scene.entities)
	{
		if (!e.visible || !e.mesh.IsValid()) continue;
		const Mesh& mesh = m_assets.GetMesh(e.mesh);

		DrawItem d;
		d.vertexBuffer = &mesh.vb;
		d.indexBuffer  = &mesh.ib;
		d.albedo = &m_assets.GetTexture(mesh.albedo);
		d.object       = hlsl::MakeObjectConstants(e.transform.World());   // ← 물체마다 다른 행렬
		m_renderScene.draws.push_back(d);
	}

	m_renderScene.envMap        = &m_assets.GetTexture(m_envMap);
	m_renderScene.irradianceMap = &m_assets.GetTexture(m_irradianceMap);
	m_renderScene.specularMap   = &m_assets.GetTexture(m_specularMap);

	m_renderScene.post.exposure       = m_exposure;
	m_renderScene.post.bloomThreshold = m_bloomThreshold;
	m_renderScene.post.bloomStrength  = m_bloomStrength;
	m_renderScene.post.bloomBlurCount = m_bloomBlurCount;

	m_renderScene.viewport = m_viewport;
	m_renderScene.scissor  = m_scissorRect;

	m_renderScene.showWireFrame = m_wireframe;
	m_renderScene.tint = m_tint;
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
	bool wireframe = m_wireframe;
	if (ImGui::Checkbox("Wireframe", &wireframe))
		m_wireframe = wireframe;
	ImGui::SliderFloat3("Tint", &m_tint.x, 0.0f, 1.0f);

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
	
	ImGui::SliderInt("Entity", &m_selected, 0, int(m_scene.entities.size()) - 1);
	if (!m_scene.entities.empty())
	{
		Entity& e = m_scene.entities[m_selected];
		ImGui::Text("%s", e.name.c_str());
		ImGui::Checkbox("Visivle", &e.visible);
		ImGui::DragFloat3("Position", &e.transform.position.x, 0.01f, -10.0f, 10.0f);
		ImGui::DragFloat3("Rotation", &e.transform.rotation.x, 0.01f, -3.14f, 3.14f);
		ImGui::DragFloat3("Scale", &e.transform.scale.x, 0.01f, 0.01f, 10.0f);
	}

	ImGui::SliderFloat3("Material FresnelR0", &m_material.fresnelR0.x, 0.0f, 1.0f);
	ImGui::SliderFloat("Material Shininess", &m_material.shininess, 1.0f, 256.0f);

	if (ImGui::RadioButton("Directional Light", m_scene.lightType == 0)) m_scene.lightType = 0;
	ImGui::SameLine();
	if (ImGui::RadioButton("Point Light", m_scene.lightType == 1)) m_scene.lightType = 1;
	ImGui::SameLine();
	if (ImGui::RadioButton("Spot Light", m_scene.lightType == 2)) m_scene.lightType = 2;

	ImGui::SliderFloat("Material Diffuse", &m_materialDiffuse, 0.0f, 1.0f);
	ImGui::SliderFloat("Material Specular", &m_materialSpecular, 0.0f, 1.0f);

	ImGui::SliderFloat3("Light Strength", &m_scene.light.strength.x, 0.0f, 5.0f);
	ImGui::SliderFloat3("Light Position", &m_scene.light.position.x, -5.0f, 5.0f);
	ImGui::SliderFloat("Light fallOffStart", &m_scene.light.fallOffStart, 0.0f, 5.0f);
	ImGui::SliderFloat("Light fallOffEnd", &m_scene.light.fallOffEnd, 0.0f, 10.0f);
	ImGui::SliderFloat("Light spotPower", &m_scene.light.spotPower, 1.0f, 512.0f);

	int newWidth = int(ImGui::GetWindowWidth());
	if (newWidth != m_guiWidth)
	{
		m_guiWidth = newWidth;
		UpdateSceneViewport();   // OnUpdate 전에 aspect까지 확정
	}
}
