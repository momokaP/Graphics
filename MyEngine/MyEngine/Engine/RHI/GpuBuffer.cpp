#include "stdafx.h"
#include "RHI/GpuBuffer.h"
#include "DXSampleHelper.h"
#include <directxtk12/ResourceUploadBatch.h>

void GpuBuffer::CreateStatic(ID3D12Device* device, DirectX::ResourceUploadBatch& upload,
    const void* data, UINT count, UINT stride,
    D3D12_RESOURCE_STATES finalState, const wchar_t* name)
{
    m_count = count;
    m_stride = stride;
    const UINT64 size = UINT64(count) * stride;

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    // 버퍼는 초기 상태를 뭘 주든 COMMON으로 만들어짐 (다른 값을 주면 디버그 레이어 경고)
    ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(m_resource.ReleaseAndGetAddressOf())));
    SetName(name);

    D3D12_SUBRESOURCE_DATA sub = {};
    sub.pData = data;
    sub.RowPitch = LONG_PTR(size);
    sub.SlicePitch = LONG_PTR(size);

    // COMMON 상태의 버퍼는 복사할 때 COPY_DEST로 '암시적 승격'됨 → 별도 배리어 불필요
    upload.Upload(m_resource.Get(), 0, &sub, 1);
    upload.Transition(m_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
    m_state = finalState;
}