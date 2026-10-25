#pragma once

#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <assimp\scene.h>
#include <iostream>
#include <string>
#include <vector>

#include "MeshData.h"
#include "Vertex.h"
#include "Paths.h"

class ModelLoader {
public:
    std::vector<MeshData> Load(const std::filesystem::path& file);
private:
    void ProcessNode(aiNode* node, const aiScene* scene, const DirectX::SimpleMath::Matrix& parentTransform);
    MeshData ProcessMesh(aiMesh* mesh, const aiScene* scene, const DirectX::SimpleMath::Matrix& transform);

public:
    std::filesystem::path m_baseDir;
    std::vector<MeshData> m_meshes;
};
