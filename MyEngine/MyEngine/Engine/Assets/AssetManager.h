#pragma once
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include "Assets/AssetHandle.h"
#include "RHI/GpuBuffer.h"
#include "RHI/Texture.h"
#include "Shaders/ShaderInterop.h"

struct MeshData;
class DescriptorHeap;
namespace DirectX { class ResourceUploadBatch; }

struct Mesh
{
	std::wstring name;
	GpuBuffer vb, ib;
	TextureHandle albedo; // 없으면 invalid → 기본 텍스처
};

struct Material
{
	std::string name;
	hlsl::MaterialConstants params = hlsl::DefaultMaterial();
	TextureHandle albedo;
};

class AssetManager
{
public:
	void Init(ID3D12Device* device, DescriptorHeap* srvHeap);

	// ── 로드: Renderer::UploadResources 람다 안에서 호출 ──
	MeshHandle CreateMesh(DirectX::ResourceUploadBatch& up, const MeshData& md, std::wstring name);
	std::vector<MeshHandle> LoadModel(DirectX::ResourceUploadBatch& up, const std::filesystem::path& path);
	TextureHandle LoadTexture(DirectX::ResourceUploadBatch& up, const std::filesystem::path& path, bool srgb);

	// ── 업로드가 끝난 뒤 한 번: 새로 로드된 텍스처의 SRV 생성 ──
	void FinalizeUploads();

	// ── 조회 ──
	const Mesh& GetMesh(MeshHandle h) const;
	const Texture& GetTexture(TextureHandle h) const; // invalid면 기본 텍스처

	void SetDefaultTexture(TextureHandle h) { m_defaultTexture = h; }
	TextureHandle DefaultTexture() const { return m_defaultTexture; }

	size_t MeshCount() const { return m_meshes.size(); }
	size_t TextureCount() const { return m_textures.size(); }
	size_t MaterialCount() const { return m_materials.size(); }

	MaterialHandle CreateMaterial(Material m);
	const Material& GetMaterial(MaterialHandle h) const; // invalid면 기본 머티리얼
	Material& EditMaterial(MaterialHandle h); // 에디터용

private:
	ID3D12Device* m_device = nullptr;
	DescriptorHeap* m_srvHeap = nullptr;

	std::deque<Mesh> m_meshes;
	std::deque<Texture> m_textures;
	std::deque<Material> m_materials;

	std::unordered_map<std::wstring, TextureHandle> m_textureCache;  // "정규화된 경로|srgb" → 핸들
	std::vector<TextureHandle> m_pendingSrv; // 업로드 뒤 SRV를 만들 목록
	TextureHandle m_defaultTexture;
	MaterialHandle m_defaultMaterial;
};