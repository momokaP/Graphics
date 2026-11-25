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
	MaterialHandle material;
	bool visible = true;
};

struct Environment
{
	TextureHandle envMap, irradianceMap, specularMap;
};

struct PostProcess
{
	float exposure = 1.0f;
	float bloomThreshold = 1.0f;
	float bloomStrength = 0.1f;
	int bloomBlurCount = 2;
};

struct Scene
{
	std::vector<Entity> entities;
	Camera camera;
	int lightType = 0; // 0 Directional, 1 Point, 2 Spot
	hlsl::Light light = hlsl::DefaultLight();

	Environment environment;
	PostProcess post;

	size_t Add(std::string name, MeshHandle mesh, MaterialHandle material, const Transform& t = {})
	{
		entities.push_back({ std::move(name), t, mesh, material, true });
		return entities.size() - 1;
	}
};