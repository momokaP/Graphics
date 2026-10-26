#include "stdafx.h"
#include "RHI/DescriptorHeap.h"
#include "DXSampleHelper.h"

void DescriptorHeap::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    UINT capacity, bool shaderVisible, const wchar_t* name)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = capacity;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));
    m_heap->SetName(name);

    m_increment = device->GetDescriptorHandleIncrementSize(type);
    m_capacity = capacity;
    m_shaderVisible = shaderVisible;
    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible)   // shader-visible이 아닌 힙에서 호출하면 디버그 레이어 에러
        m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
}

DescriptorHandle DescriptorHeap::Allocate(UINT count)
{
    UINT index;
    if (count == 1 && !m_freeList.empty())
    {
        index = m_freeList.back();
        m_freeList.pop_back();
    }
    else
    {
        if (m_top + count > m_capacity)
            throw std::runtime_error("Descriptor heap is full");   // 조용히 덮어쓰지 않고 바로 실패
        index = m_top;
        m_top += count;
    }

    DescriptorHandle h;
    h.index = index;
    h.count = count;
    h.cpu = Cpu(index);
    if (m_shaderVisible) h.gpu = Gpu(index);
    return h;
}

void DescriptorHeap::Free(DescriptorHandle& h)
{
    if (!h.IsValid()) return;
    for (UINT i = 0; i < h.count; ++i)
        m_freeList.push_back(h.index + i);
    h = {};
}

void DescriptorHeap::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu)
{
    m_freeList.push_back(UINT((cpu.ptr - m_cpuStart.ptr) / m_increment));
}