#pragma once
#include <string>
#include <vector>
#include "Scene/Transform.h"
#include "Scene/Camera.h"
#include "Shaders/ShaderInterop.h"
#include "Assets/AssetHandle.h"

struct Entity
{
	std::string name;
	Transform transform;
	MeshHandle mesh;
	bool visible = true;
};

struct Scene
{
	std::vector<Entity> entities;
	Camera camera;

	int lightType = 0; // 0 Directional, 1 Point, 2 Spot
	hlsl::Light light = hlsl::DefaultLight();

	size_t Add(std::string name, MeshHandle mesh, const Transform& t = {})
	{
		entities.push_back({ std::move(name), t, mesh, true });
		return entities.size() - 1;
	}
};