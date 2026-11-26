#pragma once
#include <d3d12.h>
#include <directxtk12/SimpleMath.h>

struct Scene;
class AssetManager;
class Camera;

struct Ray
{
	DirectX::SimpleMath::Vector3 origin;
	DirectX::SimpleMath::Vector3 direction; // 정규화됨
};

// 화면 좌표(창 클라이언트 픽셀) → 월드 공간 레이
Ray ScreenToWorldRay(const Camera& camera, bool perspective,
	const D3D12_VIEWPORT& viewport, float mouseX, float mouseY);

// 레이에 맞은 가장 가까운 엔티티 인덱스. 없으면 -1
int PickEntity(const Scene& scene, const AssetManager& assets, const Ray& ray);