#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cassert>
#include <cstring>
#include <type_traits>
#include "include/d3dx12/d3dx12.h"
#include "DXSampleHelper.h"

// CPU가 매 프레임 쓰는 상수 버퍼.
// 버퍼 하나를 frameCount개의 영역으로 나눠서, 프레임마다 다른 영역에 씀
// → GPU가 이전 프레임 영역을 읽는 동안 CPU는 다른 영역에 안전하게 씀
//
//  [ frame 0 영역 (256 정렬) | frame 1 영역 (256 정렬) ]
//    ↑ GPU가 읽는 중           ↑ CPU가 쓰는 중
template<typename T>
class FrameConstantBuffer
{
    static_assert(std::is_trivially_copyable_v<T>, "memcpy로 올리므로 단순 복사 가능한 타입이어야 함");

public:
    FrameConstantBuffer() = default;
    FrameConstantBuffer(const FrameConstantBuffer&) = delete;              // 매핑 포인터 공유 방지
    FrameConstantBuffer& operator=(const FrameConstantBuffer&) = delete;

    void Init(ID3D12Device* device, UINT frameCount, const wchar_t* name)
    {
        m_frameCount = frameCount;
        m_stride = (sizeof(T) + 255) & ~255u;   // 상수 버퍼 주소는 256바이트 정렬 규칙

        auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);   // CPU가 쓰고 GPU가 읽는 힙
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(UINT64(m_stride) * frameCount);
        ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_resource)));
        m_resource->SetName(name);

        // 계속 매핑해 둠(persistent mapping): UPLOAD 힙은 Map한 채로 둬도 됨
        CD3DX12_RANGE noRead(0, 0);              // CPU는 읽지 않음
        ThrowIfFailed(m_resource->Map(0, &noRead, reinterpret_cast<void**>(&m_mapped)));
    }

    void Write(UINT frame, const T& data)
    {
        assert(m_mapped && "FrameConstantBuffer::Init이 호출되지 않음");   // 지난번 nullptr memcpy 크래시 방지
        assert(frame < m_frameCount);
        std::memcpy(m_mapped + SIZE_T(frame) * m_stride, &data, sizeof(T));
    }

    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress(UINT frame) const
    {
        assert(m_resource && frame < m_frameCount);
        return m_resource->GetGPUVirtualAddress() + UINT64(frame) * m_stride;
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    UINT8* m_mapped = nullptr;
    UINT   m_stride = 0;
    UINT   m_frameCount = 0;
};