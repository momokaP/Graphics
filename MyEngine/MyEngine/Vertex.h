#pragma once

#include <directxtk12/SimpleMath.h>
#include <vector>

struct Vertex
{
	DirectX::SimpleMath::Vector3 position;
	DirectX::SimpleMath::Vector3 normal;
	DirectX::SimpleMath::Vector2 texcoord;
};