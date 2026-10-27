#pragma once

#include <d3d12.h>
#include <cstddef>
#include <directxtk12/SimpleMath.h>

struct Vertex
{
	DirectX::SimpleMath::Vector3 position;
	DirectX::SimpleMath::Vector3 normal;
	DirectX::SimpleMath::Vector2 texcoord;
};
static_assert(sizeof(Vertex) == 32, "Vertex layout changed: update kVertexLayout");

inline constexpr D3D12_INPUT_ELEMENT_DESC kVertexLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texcoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};