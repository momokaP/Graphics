#pragma once

#include <directxtk12/SimpleMath.h>
#include <vector>

using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;

struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 texcoord;
};