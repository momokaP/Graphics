#pragma once
#include <filesystem>
#include "RHI/GpuResource.h"
#include "RHI/DescriptorHeap.h"

namespace DirectX { class ResourceUploadBatch; }   // 헤더를 가볍게: 전방 선언만

class Texture : public GpuResource
{
public:
    // ── GPU에서 쓰는 텍스처 ──
    // 디스크립터 칸은 처음 한 번만 할당하고, 다시 만들 때(리사이즈)는 같은 칸에 뷰만 재생성
    void CreateRenderTarget(ID3D12Device* device, DescriptorHeap& rtvHeap, DescriptorHeap& srvHeap,
        UINT width, UINT height, DXGI_FORMAT format,
        const float clearColor[4], const wchar_t* name);
    void CreateDepth(ID3D12Device* device, DescriptorHeap& dsvHeap,
        UINT width, UINT height, DXGI_FORMAT format, const wchar_t* name);

    // ── 파일에서 로드 ──
    // 업로드가 끝난 뒤(upload.End().wait()) CreateSrv를 호출할 것
    bool LoadWIC(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
        const std::filesystem::path& path, bool srgb);
    void LoadDDS(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
        const std::filesystem::path& path);
    void CreateSrv(ID3D12Device* device, DescriptorHeap& srvHeap);   // 2D/큐브 자동 판별

    UINT Width() const { return UINT(m_resource->GetDesc().Width); }
    UINT Height() const { return m_resource->GetDesc().Height; }
    const float* ClearColor() const { return m_clearColor; }
    bool IsCube() const { return m_isCube; }

    DescriptorHandle srv, rtv, dsv;

private:
    void WriteSrv(ID3D12Device* device);

    float m_clearColor[4] = {};
    bool  m_isCube = false;
};