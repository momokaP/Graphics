#pragma once
#include <cstddef>

struct Scene;
class AssetManager;
struct SceneView;
class Renderer;

// 에디터가 한 프레임 동안 만지는 것들. 소유하지 않고 매 프레임 참조로 받는다.
struct EditorContext
{
	Scene& scene;
	AssetManager& assets;
	SceneView& view;
	Renderer& renderer;
	size_t lastDrawCount; // 직전 프레임 RenderScene::draws.size()
};

// 씬 편집 UI. 의존 방향: Editor → Engine (Engine은 Editor를 모른다)
class EditorUI
{
public:
	void Draw(EditorContext& ctx);
	int Selected() const { return m_selected; }

private:
	void DrawStats(EditorContext& ctx);
	void DrawOutliner(EditorContext& ctx);
	void DrawInspector(EditorContext& ctx);
	void DrawCamera(EditorContext& ctx);
	void DrawSceneSettings(EditorContext& ctx);
	void DrawRenderSettings(EditorContext& ctx);

	int m_selected = -1; // -1 = 선택 없음
	float m_frameTimes[120] = {}; // 프레임 시간 기록 (원형 버퍼)
	int m_frameIndex = 0;
};
