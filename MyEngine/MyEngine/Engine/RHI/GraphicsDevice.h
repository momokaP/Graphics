#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class GraphicsDevice
{
public:
    void Init(bool useWarp);
    void Shutdown();

    ID3D12Device* Device()  const { return m_device.Get(); }
    IDXGIFactory4* Factory() const { return m_factory.Get(); }
    ID3D12CommandQueue* Queue()   const { return m_queue.Get(); }

    UINT64 Signal();                         // 큐 끝에 펜스 신호를 넣고 그 값을 반환
    void   WaitForFence(UINT64 value);       // GPU가 value까지 끝낼 때까지 CPU 대기
    void   Flush() { WaitForFence(Signal()); }   // GPU를 완전히 비움 (리사이즈, 종료 시)

private:
    Microsoft::WRL::ComPtr<IDXGIFactory4>      m_factory;
    Microsoft::WRL::ComPtr<ID3D12Device>       m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_nextFenceValue = 1;
    HANDLE m_fenceEvent = nullptr;
};