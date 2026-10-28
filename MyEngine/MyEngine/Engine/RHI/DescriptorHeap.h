#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <climits>    

struct DescriptorHandle
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};   // shader-visible 힙에서만 유효
    UINT index = UINT_MAX;
    UINT count = 0;
    bool IsValid() const { return count != 0; }
};

class DescriptorHeap
{
public:
    void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT capacity, bool shaderVisible, const wchar_t* name);

    DescriptorHandle Allocate(UINT count = 1);    // count > 1 이면 연속된 칸 (디스크립터 테이블용)
    void Free(DescriptorHandle& handle);
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu);   // ImGui 해제 콜백용

    D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT index) const { return { m_cpuStart.ptr + SIZE_T(index) * m_increment }; }
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT index) const { return { m_gpuStart.ptr + UINT64(index) * m_increment }; }

    ID3D12DescriptorHeap* Heap() const { return m_heap.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    UINT m_increment = 0;
    UINT m_capacity = 0;
    UINT m_top = 0;                 // 아직 한 번도 안 쓴 칸의 시작
    bool m_shaderVisible = false;
    std::vector<UINT> m_freeList;   // 반납된 단일 칸
};