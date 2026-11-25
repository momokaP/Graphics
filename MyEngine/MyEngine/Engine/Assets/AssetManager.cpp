#include "stdafx.h"
#include "Assets/AssetManager.h"
#include <cassert>
#include <cwchar>
#include <directxtk12/ResourceUploadBatch.h>
#include "MeshData.h"
#include "GeometryGenerator.h"
#include "RHI/DescriptorHeap.h"

namespace fs = std::filesystem;

void AssetManager::Init(ID3D12Device* device, DescriptorHeap* srvHeap)
{
	m_device = device;
	m_srvHeap = srvHeap;
	m_defaultMaterial = CreateMaterial({ "Default" }); // 머티리얼 없는 엔티티용
}

MeshHandle AssetManager::CreateMesh(DirectX::ResourceUploadBatch& up, const MeshData& md, std::wstring name)
{
	Mesh& m = m_meshes.emplace_back();
	m.name = std::move(name);
	m.vb.CreateVertexBuffer(m_device, up, md.vertices, (m.name + L" VB").c_str());
	m.ib.CreateIndexBuffer(m_device, up, md.indices, (m.name + L" IB").c_str());

	if (!md.albedoTexturePath.empty())
		m.albedo = LoadTexture(up, md.albedoTexturePath, true);

	return { uint32_t(m_meshes.size() - 1) };
}

std::vector<MeshHandle> AssetManager::LoadModel(DirectX::ResourceUploadBatch& up, const std::filesystem::path& path)
{
	std::vector<MeshHandle> handles;
	const std::vector<MeshData> datas = GeometryGenerator::ReadFromFile(path);
	for (size_t i = 0; i < datas.size(); ++i)
		handles.push_back(CreateMesh(up, datas[i], path.stem().wstring() + L"[" + std::to_wstring(i) + L"]"));
	return handles;
}

TextureHandle AssetManager::LoadTexture(DirectX::ResourceUploadBatch& up, const std::filesystem::path& path, bool srgb)
{
	// 1) 캐시 키: 정규화된 경로 + 색공간 (같은 파일도 sRGB/선형이면 다른 텍스처)
	std::error_code ec;
	const fs::path canon = fs::weakly_canonical(path, ec);
	const std::wstring key = (ec ? path : canon).wstring() + (srgb ? L"|srgb" : L"|linear");

	if (auto it = m_textureCache.find(key); it != m_textureCache.end())
		return it->second; // 이미 있음 → 재사용

	if (!fs::exists(path))
		return {}; // invalid → GetTexture가 기본 텍스처를 줌

	// 2) 새로 로드
	Texture& tex = m_textures.emplace_back();
	const bool isDds = _wcsicmp(path.extension().c_str(), L".dds") == 0;
	if (isDds)
		tex.LoadDDS(m_device, up, path);
	else if (!tex.LoadWIC(m_device, up, path, srgb))
	{
		m_textures.pop_back();
		return {};
	}

	const TextureHandle h{ uint32_t(m_textures.size() - 1) };
	m_textureCache.emplace(key, h);
	m_pendingSrv.push_back(h);
	return h;
}

void AssetManager::FinalizeUploads()
{
	for (TextureHandle h : m_pendingSrv)
		m_textures[h.index].CreateSrv(m_device, *m_srvHeap);
	m_pendingSrv.clear();
}

const Mesh& AssetManager::GetMesh(MeshHandle h) const
{
	assert(h.IsValid() && h.index < m_meshes.size() && "잘못된 MeshHandle");
	return m_meshes[h.index];
}

const Texture& AssetManager::GetTexture(TextureHandle h) const
{
	if (!h.IsValid()) h = m_defaultTexture;
	assert(h.IsValid() && h.index < m_textures.size() && "잘못된 TextureHandle (기본 텍스쳐도 없음)");
	return m_textures[h.index];
}

MaterialHandle AssetManager::CreateMaterial(Material m)
{
	m_materials.push_back(std::move(m));
	return { uint32_t(m_materials.size() - 1) };
}

const Material& AssetManager::GetMaterial(MaterialHandle h) const
{
	if (!h.IsValid()) h = m_defaultMaterial;
	assert(h.index < m_materials.size() && "잘못된 MaterialHandle");
	return m_materials[h.index];
}

Material& AssetManager::EditMaterial(MaterialHandle h)
{
	if (!h.IsValid()) h = m_defaultMaterial;
	assert(h.index < m_materials.size() && "잘못된 MaterialHandle");
	return m_materials[h.index];
}
