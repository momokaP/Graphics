#include "stdafx.h"
#include "RHI/LinearUploadAllocator.h"
#include "DXSampleHelper.h"
#include <stdexcept>

void LinearUploadAllocator::Init(ID3D12Device* device, UINT frameCount, UINT64 bytesPerFrame, const wchar_t* name)
{
    m_frameCount = frameCount;
    m_bytesPerFrame = (bytesPerFrame + 255) & ~UINT64(255);   // 프레임 영역 시작도 256 정렬이 되도록

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(m_bytesPerFrame * frameCount);
    ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource)));
    m_resource->SetName(name);

    CD3DX12_RANGE noRead(0, 0);
    ThrowIfFailed(m_resource->Map(0, &noRead, reinterpret_cast<void**>(&m_mapped)));   // 계속 매핑
    m_gpuBase = m_resource->GetGPUVirtualAddress();
}

void LinearUploadAllocator::BeginFrame(UINT frameIndex)
{
    assert(frameIndex < m_frameCount);
    m_frameBase = UINT64(frameIndex) * m_bytesPerFrame;
    m_offset = 0;
}

LinearUploadAllocator::Allocation LinearUploadAllocator::Allocate(UINT64 size, UINT64 alignment)
{
    assert(m_mapped && "LinearUploadAllocator::Init이 호출되지 않음");
    assert((alignment & (alignment - 1)) == 0 && "alignment는 2의 거듭제곱");

    const UINT64 aligned = (m_offset + alignment - 1) & ~(alignment - 1);   // 올림 정렬
    if (aligned + size > m_bytesPerFrame)
        throw std::runtime_error("LinearUploadAllocator: 프레임 용량 초과 (bytesPerFrame을 늘리세요)");

    m_offset = aligned + size;

    Allocation a;
    a.cpu = m_mapped + m_frameBase + aligned;
    a.gpu = m_gpuBase + m_frameBase + aligned;
    return a;
}