#pragma once

#include <directxtk12/SimpleMath.h>
#include <vector>

using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;

struct Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texcoord;
    Vector3 color;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
};

class GeometryGenerator {
public:
    static MeshData MakeSquare();
    static MeshData MakeBox();
    static MeshData MakeCylinder(const float bottomRadius,
        const float topRadius, float height,
        int sliceCount);
};
