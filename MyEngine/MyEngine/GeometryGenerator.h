#pragma once

#include <directxtk12/SimpleMath.h>
#include <vector>

#include "Vertex.h"
#include "MeshData.h"
#include "ModelLoader.h"

using namespace std;

using namespace DirectX;
using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;

class GeometryGenerator {
public:
    static MeshData MakeSquare();
    static MeshData MakeBox();
    static MeshData MakeCylinder(const float bottomRadius,
        const float topRadius, float height,
        int numSlices);
    static MeshData MakeSphere(const float radius, const int numSlices,
        const int numStacks);
    static MeshData MakeTetrahedron();
    static MeshData MakeIcosahedron();
    static MeshData SubdivideToSphere(const float radius, MeshData meshData);
    static vector<MeshData> ReadFromFile(std::string basePath, std::string filename);
};
