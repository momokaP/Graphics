#include "ModelLoader.h"

#include <filesystem>

std::vector<MeshData> ModelLoader::Load(const std::string& basePath, const std::string& fileName)
{
    m_basePath = basePath;
    m_meshes.clear();
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(basePath + fileName,
        aiProcess_Triangulate |            // 사각형 이상 → 삼각형
        aiProcess_ConvertToLeftHanded |    // D3D 좌표계 + UV 상하 + 감김 순서
        aiProcess_GenSmoothNormals |       // 노멀 없으면 생성
        aiProcess_JoinIdenticalVertices);  // 중복 정점 병합 → 인덱스 활용

    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
        throw std::runtime_error(importer.GetErrorString());

    ProcessNode(scene->mRootNode, scene, Matrix());
    return m_meshes;
}

void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene, const Matrix& parentTransform)
{
    // aiMatrix4x4는 행 우선(row-major) + 열벡터 규약 → SimpleMath용으로 전치
    Matrix local(&node->mTransformation.a1);
    Matrix world = local.Transpose() * parentTransform;

    for (UINT i = 0; i < node->mNumMeshes; i++)
        m_meshes.push_back(ProcessMesh(scene->mMeshes[node->mMeshes[i]], scene, world));

    for (UINT i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene, world);
}

MeshData ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene, const Matrix& transform)
{
    MeshData out;
    out.vertices.reserve(mesh->mNumVertices);

    Matrix normalMat = transform;
    normalMat.Translation(Vector3(0.0f));
    normalMat = normalMat.Invert().Transpose();

    for (UINT i = 0; i < mesh->mNumVertices; i++) {
        Vertex v{};
        v.position = Vector3::Transform(Vector3(&mesh->mVertices[i].x), transform);
        v.normal = Vector3::TransformNormal(Vector3(&mesh->mNormals[i].x), normalMat);
        v.normal.Normalize();
        if (mesh->mTextureCoords[0])
            v.texcoord = Vector2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        out.vertices.push_back(v);
    }

    for (UINT f = 0; f < mesh->mNumFaces; f++) {
        const aiFace& face = mesh->mFaces[f];
        for (UINT j = 0; j < face.mNumIndices; j++)
            out.indices.push_back(face.mIndices[j]);
    }

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        aiString path;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
            out.albedoTexturePath = m_basePath + std::filesystem::path(path.C_Str()).filename().string();
    }
    return out;
}
