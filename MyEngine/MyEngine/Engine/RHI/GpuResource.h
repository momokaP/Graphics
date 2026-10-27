#pragma once
#include <d3d12.h>
#include <wrl.h>

class GpuResource
{
public:
    virtual ~GpuResource() = default;

    ID3D12Resource* Get() const { return m_resource.Get(); }
    D3D12_RESOURCE_STATES State() const { return m_state; }
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const { return m_resource->GetGPUVirtualAddress(); }
    bool IsValid() const { return m_resource != nullptr; }

    // 외부에서 만든 리소스(스왑체인 백버퍼 등)를 '현재 상태'와 함께 넘겨받음
    void Attach(Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES currentState);
    void Reset();
    void SetName(const wchar_t* name);

    // 현재 상태와 같으면 아무것도 안 함
    void Transition(ID3D12GraphicsCommandList* cmd, D3D12_RESOURCE_STATES newState);

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;
};