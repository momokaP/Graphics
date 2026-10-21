#pragma once

#include "ModelLoader.h"

#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <assimp\scene.h>
#include <iostream>
#include <string>
#include <vector>

#include "MeshData.h"
#include "Vertex.h"

using DirectX::SimpleMath::Matrix;

class ModelLoader {
public:
    std::vector<MeshData> Load(const std::string& basePath, const std::string& fileName);
private:
    void ProcessNode(aiNode* node, const aiScene* scene, const Matrix& parentTransform);
    MeshData ProcessMesh(aiMesh* mesh, const aiScene* scene, const Matrix& transform);

public:
    std::string m_basePath;
    std::vector<MeshData> m_meshes;
};
