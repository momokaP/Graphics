#include "stdafx.h"
#include "RHI/GpuResource.h"

void GpuResource::Attach(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES currentState)
{
    m_resource = std::move(resource);
    m_state = currentState;
}

void GpuResource::Reset()
{
    m_resource.Reset();
    m_state = D3D12_RESOURCE_STATE_COMMON;
}

void GpuResource::SetName(const wchar_t* name)
{
    if (m_resource && name) m_resource->SetName(name);
}

void GpuResource::Transition(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES newState)
{
    if (m_state == newState) return;
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_resource.Get(), m_state, newState);
    cmd->ResourceBarrier(1, &barrier);
    m_state = newState;
}