#include "stdafx.h"
#include "Scene/SceneExtract.h"
#include "Scene/Scene.h"
#include "Assets/AssetManager.h"
#include "Renderer/RenderScene.h"

using DirectX::SimpleMath::Matrix;

void ExtractRenderScene(const Scene& scene, const AssetManager& assets, const SceneView& view, RenderScene& out)
{
	// ── 프레임 상수: 카메라 ──
	const Matrix v = scene.camera.View();
	const Matrix p = scene.camera.Proj(view.perspective);
	out.frame.view = v;
	out.frame.proj = p;
	out.frame.viewProj = v * p;
	out.frame.eyeWorld = scene.camera.Position();

	// ── 프레임 상수: 조명 (선택한 1개만 켬) ──
	for (size_t i = 0; i < std::size(out.frame.lights); ++i)
		out.frame.lights[i] = (i == size_t(scene.lightType)) ? scene.light : hlsl::Light();

	// ── Draw 목록 ──
	out.draws.clear();
	for (const Entity& e : scene.entities)
	{
		if (!e.visible || !e.mesh.IsValid()) continue;
		// (나중에 프러스텀 컬링이 들어갈 자리)

		const Mesh& mesh = assets.GetMesh(e.mesh);
		const Material& mat = assets.GetMaterial(e.material);

		DrawItem d;
		d.vertexBuffer = &mesh.vb;
		d.indexBuffer = &mesh.ib;
		d.object = hlsl::MakeObjectConstants(e.transform.World());
		d.material = mat.params;
		d.albedo = &assets.GetTexture(mat.albedo.IsValid() ? mat.albedo : mesh.albedo);
		out.draws.push_back(d);
	}

	// ── 환경 ──
	out.envMap = &assets.GetTexture(scene.environment.envMap);
	out.irradianceMap = &assets.GetTexture(scene.environment.irradianceMap);
	out.specularMap = &assets.GetTexture(scene.environment.specularMap);

	// ── 후처리 (씬 설정 → 렌더러 설정) ──
	out.post.exposure = scene.post.exposure;
	out.post.bloomThreshold = scene.post.bloomThreshold;
	out.post.bloomStrength = scene.post.bloomStrength;
	out.post.bloomBlurCount = scene.post.bloomBlurCount;

	// ── 뷰 ──
	out.viewport = view.viewport;
	out.scissor = view.scissor;
	out.showWireFrame = view.wireframe;
	out.tint = view.wireColor;

}
