#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cassert>
#include <cstring>
#include <type_traits>

// 프레임마다 앞에서부터 잘라 쓰는 UPLOAD 버퍼.
// 프레임 시작(BeginFrame)에 offset을 0으로 되돌리기만 하므로 개별 해제가 없음.
// 안전한 이유: MoveToNextFrame이 "이 프레임 슬롯을 GPU가 다 썼다"를 보장한 뒤에 BeginFrame이 호출됨.
//
//  리소스 하나 = [ frame 0 영역 | frame 1 영역 ]
//                 └ offset → (앞으로만 증가)
class LinearUploadAllocator
{
public:
    struct Allocation
    {
        void* cpu = nullptr;                  // CPU가 쓸 주소
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;    // 루트 CBV에 넘길 주소
    };

    LinearUploadAllocator() = default;
    LinearUploadAllocator(const LinearUploadAllocator&) = delete;
    LinearUploadAllocator& operator=(const LinearUploadAllocator&) = delete;

    void Init(ID3D12Device* device, UINT frameCount, UINT64 bytesPerFrame, const wchar_t* name);

    // 매 프레임 기록 시작 시 호출: 해당 프레임 영역의 처음으로 되돌림
    void BeginFrame(UINT frameIndex);

    // size바이트를 alignment 정렬로 잘라 줌 (상수 버퍼는 256)
    Allocation Allocate(UINT64 size, UINT64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    // 가장 많이 쓰는 형태: 구조체 하나를 올리고 GPU 주소를 돌려줌
    template<typename T>
    D3D12_GPU_VIRTUAL_ADDRESS Push(const T& data)
    {
        static_assert(std::is_trivially_copyable_v<T>, "memcpy로 올리므로 단순 복사 가능한 타입이어야 함");
        Allocation a = Allocate(sizeof(T));
        std::memcpy(a.cpu, &data, sizeof(T));
        return a.gpu;
    }

    UINT64 UsedBytes() const { return m_offset; }          // 이번 프레임에 쓴 양 (GUI 표시용)
    UINT64 CapacityPerFrame() const { return m_bytesPerFrame; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    UINT8* m_mapped = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuBase = 0;
    UINT64 m_bytesPerFrame = 0;
    UINT64 m_frameBase = 0;      // 현재 프레임 영역의 시작 (리소스 기준)
    UINT64 m_offset = 0;         // 현재 프레임 영역 안에서 다음에 자를 위치
    UINT   m_frameCount = 0;
};