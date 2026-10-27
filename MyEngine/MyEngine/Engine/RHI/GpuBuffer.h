#pragma once
#include <vector>
#include "RHI/GpuResource.h"

namespace DirectX { class ResourceUploadBatch; }

class GpuBuffer : public GpuResource
{
public:
    // DEFAULT 힙 생성 + ResourceUploadBatch로 복사 (data는 호출 시점에 복사되므로 바로 해제해도 됨)
    void CreateStatic(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
        const void* data, UINT count, UINT stride,
        D3D12_RESOURCE_STATES finalState, const wchar_t* name);

    template<typename T>
    void CreateVertexBuffer(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
        const std::vector<T>& v, const wchar_t* name)
    {
        CreateStatic(device, upload, v.data(), UINT(v.size()), sizeof(T),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, name);
    }

    template<typename T>
    void CreateIndexBuffer(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
        const std::vector<T>& v, const wchar_t* name)
    {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4, "Index must be 16 or 32 bit");
        CreateStatic(device, upload, v.data(), UINT(v.size()), sizeof(T),
            D3D12_RESOURCE_STATE_INDEX_BUFFER, name);
    }

    UINT Count() const { return m_count; }
    UINT SizeInBytes() const { return m_count * m_stride; }

    D3D12_VERTEX_BUFFER_VIEW VertexView() const { return { GpuAddress(), SizeInBytes(), m_stride }; }
    D3D12_INDEX_BUFFER_VIEW  IndexView() const
    {
        return { GpuAddress(), SizeInBytes(), m_stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
    }

private:
    UINT m_count = 0;
    UINT m_stride = 0;
};