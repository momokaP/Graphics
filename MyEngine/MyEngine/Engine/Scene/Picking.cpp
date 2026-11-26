#include "stdafx.h"
#include "Scene/Picking.h"

#include <cfloat>
#include "Scene/Scene.h"
#include "Assets/AssetManager.h"

using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Matrix;

Ray ScreenToWorldRay(const Camera& camera, bool perspective, const D3D12_VIEWPORT& viewport, float mouseX, float mouseY)
{
	// ① 픽셀 → NDC. 뷰포트가 GUI 패널 폭만큼 오른쪽에 있으니 TopLeftX를 뺌
	const float ndcX = ((mouseX - viewport.TopLeftX) / viewport.Width) * 2.0f - 1.0f;
	const float ndcY = 1.0f - ((mouseY - viewport.TopLeftY) / viewport.Height) * 2.0f; // 화면 y는 아래로, NDC y는 위로

	// ② NDC의 근평면(z=0)과 원평면(z=1) 점을 월드로 되돌림
	const Matrix invViewProj = (camera.View() * camera.Proj(perspective)).Invert();
	const Vector3 nearPoint = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), invViewProj);   // w 나누기 포함
	const Vector3 farPoint = Vector3::Transform(Vector3(ndcX, ndcY, 1.0f), invViewProj);

	Ray ray;
	ray.origin = nearPoint;
	ray.direction = farPoint - nearPoint;
	ray.direction.Normalize();
	return ray;
}

int PickEntity(const Scene& scene, const AssetManager& assets, const Ray& ray)
{
	int best = -1;
	float bestDistance = FLT_MAX;
	
	for (int i = 0; i<int(scene.entities.size()); ++i)
	{
		const Entity& e = scene.entities[i];
		if (!e.visible || !e.mesh.IsValid()) continue;

		// ③ 레이를 엔티티 로컬 공간으로 (상자를 월드로 옮기는 대신 레이를 옮김)
		const Matrix invWorld = e.transform.World().Invert();
		const Vector3 localOrigin = Vector3::Transform(ray.origin, invWorld);
		Vector3 localDir = Vector3::TransformNormal(ray.direction, invWorld);
		const float scale = localDir.Length(); // 스케일 때문에 길이가 1이 아님
		localDir /= scale; // Intersects는 정규화된 방향을 요구

		float localDistance = 0.0f;
		if (assets.GetMesh(e.mesh).localBounds.Intersects(localOrigin, localDir, localDistance))
		{
			const float worldDistance = localDistance / scale;
			if (worldDistance < bestDistance)
			{
				bestDistance = worldDistance;
				best = i;
			}
		}
	}
	return best;
}
