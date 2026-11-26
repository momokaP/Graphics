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
			m_scene.environment.envMap = m_assets.LoadTexture(up, ibl / L"DayEnvironmentHDRI057EnvHDR.dds", false);
			m_scene.environment.irradianceMap = m_assets.LoadTexture(up, ibl / L"DayEnvironmentHDRI057DiffuseHDR.dds", false);
			m_scene.environment.specularMap = m_assets.LoadTexture(up, ibl / L"DayEnvironmentHDRI057SpecularHDR.dds", false);
		});
	m_assets.FinalizeUploads();      // 업로드가 끝난 뒤 SRV 생성

	Material texturedMat{ "Textured" };
	texturedMat.params.useTexture = 1;
	texturedMat.params.diffuse = Vector3(0.8f);
	texturedMat.params.specular = Vector3(0.2f);
	const MaterialHandle textured = m_assets.CreateMaterial(texturedMat);


	Material shinyMat{ "Shiny" };
	shinyMat.params.diffuse = Vector3(0.9f, 0.3f, 0.2f);
	shinyMat.params.specular = Vector3(1.0f);
	shinyMat.params.shininess = 128.0f;
	const MaterialHandle shiny = m_assets.CreateMaterial(shinyMat);

	Material matteMat("Matte");
	matteMat.params.diffuse = Vector3(0.5f);
	matteMat.params.specular = Vector3(0.0f);
	const MaterialHandle matte = m_assets.CreateMaterial(matteMat);

	Transform tBox;
	tBox.position = { -0.8f, 0.0f, 0.0f };
	tBox.scale = Vector3(0.4f);
	m_scene.Add("Box", box, textured, tBox);

	Transform tSphere;
	tSphere.position = { 0.8f, 0.0f, 0.0f };
	tSphere.scale = Vector3(0.4f);
	m_scene.Add("Sphere", sphere, shiny, tSphere);

	Transform tGround;
	tGround.position = { 0.0f, -0.5f, 0.0f };
	tGround.rotation.x = DirectX::XM_PIDIV2;
	tGround.scale = Vector3(3.0f);
	m_scene.Add("Ground", ground, matte, tGround);
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
	const bool flying = !io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Right);
	if (flying)
	{
		in.look = true;
		in.mouseDx = io.MouseDelta.x;
		in.mouseDy = io.MouseDelta.y;
		in.forward = float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S));
		in.right = float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A));
		in.up = float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q));
		in.fast = io.KeyShift;
	}
	m_scene.camera.SetAspect(m_sceneAspect);
	m_scene.camera.Update(dt, in);
	
	m_view.viewport = m_viewport;
	m_view.scissor = m_scissorRect;
	ExtractRenderScene(m_scene, m_assets, m_view, m_renderScene);
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
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "SandboxApp (Renderer)");

	EditorContext ctx{ m_scene, m_assets, m_view, m_renderer, m_renderScene.draws.size() };
	m_editor.Draw(ctx);

	// 패널 폭이 바뀌면 씬 뷰포트 재계산 (앱의 일: 창 레이아웃)
	int newWidth = int(ImGui::GetWindowWidth());
	if (newWidth != m_guiWidth)
	{
		m_guiWidth = newWidth;
		UpdateSceneViewport();   // OnUpdate 전에 aspect까지 확정
	}
}
