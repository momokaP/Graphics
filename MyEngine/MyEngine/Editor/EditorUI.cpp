#include "stdafx.h"
#include "Editor/EditorUI.h" 인식 못함

#include <imgui.h>
#include <string>
#include <algorithm>

#include "Scene/Scene.h"
#include "Scene/SceneExtract.h"
#include "Assets/AssetManager.h"
#include "Renderer/Renderer.h"

using DirectX::SimpleMath::Vector3;

namespace
{
	constexpr float kRadToDeg = 57.29577951f;

	// Mesh::name은 wstring, ImGui는 UTF-8 → 변환
	std::string ToUtf8(const std::wstring& w)
	{
		if (w.empty()) return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
		std::string s(n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
		return s;
	}

	MeshHandle MeshAt(size_t i) { MeshHandle h; h.index = uint32_t(i); return h; }
	MaterialHandle MaterialAt(size_t i) { MaterialHandle h; h.index = uint32_t(i); return h; }
}

void EditorUI::Draw(EditorContext& ctx)
{
	// 엔티티가 바깥에서 줄었을 때를 대비해 선택 인덱스를 범위 안으로
	m_selected = std::min(m_selected, int(ctx.scene.entities.size()) - 1);

	DrawStats(ctx);

	if (ImGui::CollapsingHeader("Outliner", ImGuiTreeNodeFlags_DefaultOpen))  DrawOutliner(ctx);
	if (ImGui::CollapsingHeader("Inspector", ImGuiTreeNodeFlags_DefaultOpen)) DrawInspector(ctx);
	if (ImGui::CollapsingHeader("Camera"))                                    DrawCamera(ctx);
	if (ImGui::CollapsingHeader("Scene Settings"))                            DrawSceneSettings(ctx);
	if (ImGui::CollapsingHeader("Render"))                                    DrawRenderSettings(ctx);
}

void EditorUI::DrawStats(EditorContext& ctx)
{
	const ImGuiIO& io = ImGui::GetIO();
	m_frameTimes[m_frameIndex] = io.DeltaTime * 1000.0f;
	m_frameIndex = (m_frameIndex + 1) % IM_ARRAYSIZE(m_frameTimes);

	ImGui::Text("%.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
	ImGui::PlotLines("##frametime", m_frameTimes, IM_ARRAYSIZE(m_frameTimes), m_frameIndex,
		nullptr, 0.0f, 20.0f, ImVec2(-1.0f, 40.0f));   // 0~20ms 범위, 스파이크가 보이도록
	ImGui::Text("CPU wait for GPU: %.2f ms", ctx.renderer.LastGpuWaitMs());
	ImGui::Text("Upload: %.1f KB", ctx.renderer.UploadBytesUsed() / 1024.0);
	ImGui::Text("Draws: %zu  (entities %zu)", ctx.lastDrawCount, ctx.scene.entities.size());
	ImGui::Text("Meshes %zu  Textures %zu  Materials %zu",
		ctx.assets.MeshCount(), ctx.assets.TextureCount(), ctx.assets.MaterialCount());
}

void EditorUI::DrawOutliner(EditorContext& ctx)
{
	auto& ents = ctx.scene.entities;
	int pendingDelete = -1;
	int pendingDuplicate = -1;

	// ── 버튼 ──
	if (ImGui::Button("Add")) ImGui::OpenPopup("AddEntity");
	ImGui::SameLine();
	ImGui::BeginDisabled(m_selected < 0);
	if (ImGui::Button("Duplicate")) pendingDuplicate = m_selected;
	ImGui::SameLine();
	if (ImGui::Button("Delete"))    pendingDelete = m_selected;
	ImGui::EndDisabled();

	// ── Add: 메시를 골라 새 엔티티 생성 ──
	if (ImGui::BeginPopup("AddEntity"))
	{
		for (size_t i = 0; i < ctx.assets.MeshCount(); ++i)
		{
			const std::string name = ToUtf8(ctx.assets.GetMesh(MeshAt(i)).name);
			ImGui::PushID(int(i));
			if (ImGui::Selectable(name.c_str()))
				m_selected = int(ctx.scene.Add(name, MeshAt(i), MaterialHandle{}));   // 기본 머티리얼
			ImGui::PopID();
		}
		ImGui::EndPopup();
	}

	// ── 엔티티 목록 ──
	ImGui::BeginChild("##entityList", ImVec2(0.0f, 150.0f), true);
	for (int i = 0; i < int(ents.size()); ++i)
	{
		Entity& e = ents[i];
		ImGui::PushID(i);                      // ← 이름이 같은 엔티티가 있어도 ID가 겹치지 않게

		ImGui::Checkbox("##visible", &e.visible);
		ImGui::SameLine();
		if (ImGui::Selectable(e.name.c_str(), m_selected == i))
			m_selected = i;

		if (ImGui::BeginPopupContextItem())    // 방금 그린 Selectable에 우클릭 메뉴
		{
			m_selected = i;
			if (ImGui::MenuItem("Duplicate")) pendingDuplicate = i;
			if (ImGui::MenuItem("Delete"))    pendingDelete = i;
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}
	ImGui::EndChild();

	// ── Delete 키 (텍스트 입력 중에는 무시) ──
	if (m_selected >= 0 && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
		pendingDelete = m_selected;

	// ── 변경은 루프가 끝난 뒤에 적용 ──
	if (pendingDuplicate >= 0)
	{
		Entity copy = ents[pendingDuplicate];  // 핸들도 복사 → 같은 메시·머티리얼 공유
		copy.name += " (copy)";
		copy.transform.position.x += 0.5f;
		ents.push_back(copy);
		m_selected = int(ents.size()) - 1;
	}
	else if (pendingDelete >= 0)
	{
		ents.erase(ents.begin() + pendingDelete);
		if (m_selected > pendingDelete)       --m_selected;                    // 뒤쪽이 한 칸씩 당겨짐
		else if (m_selected == pendingDelete) m_selected = std::min(m_selected, int(ents.size()) - 1);
	}
}

void EditorUI::DrawInspector(EditorContext& ctx)
{
	if (m_selected < 0) { ImGui::TextDisabled("(no selection)"); return; }
	Entity& e = ctx.scene.entities[m_selected];

	// ── 기본 ──
	char nameBuf[128];
	strncpy_s(nameBuf, e.name.c_str(), _TRUNCATE);
	if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
		e.name = nameBuf;
	ImGui::Checkbox("Visible", &e.visible);

	// ── Transform ──
	ImGui::SeparatorText("Transform");
	ImGui::DragFloat3("Position", &e.transform.position.x, 0.01f);
	Vector3 deg = e.transform.rotation * kRadToDeg;                // 저장은 라디안, 표시는 도
	if (ImGui::DragFloat3("Rotation", &deg.x, 0.5f))
		e.transform.rotation = deg / kRadToDeg;
	ImGui::DragFloat3("Scale", &e.transform.scale.x, 0.01f, 0.001f, 100.0f);   // 0 방지 (노멀 행렬)
	if (ImGui::Button("Reset Transform")) e.transform = Transform{};

	// ── Mesh ──
	ImGui::SeparatorText("Mesh");
	const std::string meshName = e.mesh.IsValid() ? ToUtf8(ctx.assets.GetMesh(e.mesh).name) : "(none)";
	if (ImGui::BeginCombo("Mesh", meshName.c_str()))
	{
		for (size_t i = 0; i < ctx.assets.MeshCount(); ++i)
		{
			const bool selected = (e.mesh == MeshAt(i));
			ImGui::PushID(int(i));
			if (ImGui::Selectable(ToUtf8(ctx.assets.GetMesh(MeshAt(i)).name).c_str(), selected))
				e.mesh = MeshAt(i);
			if (selected) ImGui::SetItemDefaultFocus();
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	// ── Material ──
	ImGui::SeparatorText("Material");
	if (ImGui::BeginCombo("Material", ctx.assets.GetMaterial(e.material).name.c_str()))
	{
		for (size_t i = 0; i < ctx.assets.MaterialCount(); ++i)
		{
			const bool selected = (e.material == MaterialAt(i));
			ImGui::PushID(int(i));
			if (ImGui::Selectable(ctx.assets.GetMaterial(MaterialAt(i)).name.c_str(), selected))
				e.material = MaterialAt(i);
			if (selected) ImGui::SetItemDefaultFocus();
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	// 머티리얼은 공유된다 (3-4) → 고치면 같은 머티리얼을 쓰는 물체가 모두 바뀐다
	const auto users = std::count_if(ctx.scene.entities.begin(), ctx.scene.entities.end(),
		[&](const Entity& x) { return x.material == e.material; });
	if (users > 1)
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Shared by %d entities", int(users));

	Material& mat = ctx.assets.EditMaterial(e.material);
	auto& p = mat.params;
	ImGui::ColorEdit3("Diffuse", &p.diffuse.x);
	ImGui::ColorEdit3("Specular", &p.specular.x);
	ImGui::SliderFloat("Shininess", &p.shininess, 1.0f, 256.0f);
	bool useTex = p.useTexture != 0;
	if (ImGui::Checkbox("Use Texture", &useTex)) p.useTexture = useTex ? 1u : 0u;

	if (ImGui::TreeNode("Rim / Fresnel"))
	{
		ImGui::SliderFloat("Rim Strength", &p.rimStrength, 0.0f, 10.0f);
		ImGui::SliderFloat("Rim Power", &p.rimPower, 0.01f, 10.0f);
		ImGui::ColorEdit3("Rim Color", &p.rimColor.x);
		bool smooth = p.useSmoothstep != 0;
		if (ImGui::Checkbox("Use Smoothstep", &smooth)) p.useSmoothstep = smooth ? 1u : 0u;
		ImGui::SliderFloat3("Fresnel R0", &p.fresnelR0.x, 0.0f, 1.0f);
		ImGui::TreePop();
	}
}

void EditorUI::DrawCamera(EditorContext& ctx)
{
	Camera& cam = ctx.scene.camera;

	Vector3 pos = cam.Position();
	if (ImGui::DragFloat3("Position##cam", &pos.x, 0.01f)) cam.SetPosition(pos);

	float yawPitch[2] = { cam.Yaw() * kRadToDeg, cam.Pitch() * kRadToDeg };
	if (ImGui::DragFloat2("Yaw / Pitch", yawPitch, 0.5f))
		cam.SetYawPitch(yawPitch[0] / kRadToDeg, yawPitch[1] / kRadToDeg);

	float fov = cam.FovY();
	if (ImGui::SliderFloat("FOV Y", &fov, 20.0f, 120.0f, "%.0f deg")) cam.SetFovY(fov);
	ImGui::SliderFloat("Move Speed", &cam.moveSpeed, 0.1f, 20.0f, "%.1f m/s", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Look Speed", &cam.lookSpeed, 0.0005f, 0.01f, "%.4f");

	if (ImGui::Button("Reset Camera"))
	{
		cam.SetPosition(Vector3(0.0f, 0.0f, -2.0f));
		cam.SetYawPitch(0.0f, 0.0f);
	}
	ImGui::TextDisabled("RMB + WASD / QE, Shift = fast");
}

void EditorUI::DrawSceneSettings(EditorContext& ctx)
{
	Scene& s = ctx.scene;

	ImGui::SeparatorText("Light");
	ImGui::RadioButton("Directional", &s.lightType, 0); ImGui::SameLine();
	ImGui::RadioButton("Point", &s.lightType, 1);       ImGui::SameLine();
	ImGui::RadioButton("Spot", &s.lightType, 2);
	ImGui::SliderFloat3("Strength", &s.light.strength.x, 0.0f, 5.0f);
	ImGui::SliderFloat3("Position##light", &s.light.position.x, -5.0f, 5.0f);
	ImGui::SliderFloat("FallOff Start", &s.light.fallOffStart, 0.0f, 5.0f);
	ImGui::SliderFloat("FallOff End", &s.light.fallOffEnd, 0.0f, 10.0f);
	ImGui::SliderFloat("Spot Power", &s.light.spotPower, 1.0f, 512.0f);

	ImGui::SeparatorText("Post Process");
	ImGui::SliderFloat("Exposure", &s.post.exposure, 0.1f, 5.0f);
	ImGui::SliderFloat("Bloom Threshold", &s.post.bloomThreshold, 0.0f, 3.0f);
	ImGui::SliderFloat("Bloom Strength", &s.post.bloomStrength, 0.0f, 3.0f);
	ImGui::SliderInt("Bloom Blur Count", &s.post.bloomBlurCount, 0, 8);
}

void EditorUI::DrawRenderSettings(EditorContext& ctx)
{
	ImGui::Checkbox("Wireframe", &ctx.view.wireframe);
	ImGui::ColorEdit3("Wireframe Color", &ctx.view.wireColor.x);
	ImGui::Checkbox("Perspective", &ctx.view.perspective);

	bool vsync = ctx.renderer.VSync();
	if (ImGui::Checkbox("VSync", &vsync)) ctx.renderer.SetVSync(vsync);
	if (!ctx.renderer.IsTearingSupported())
		ImGui::TextDisabled("(tearing not supported)");
}
