#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>
#include "RHI/DescriptorHeap.h"

class GraphicsDevice;

class SwapChain
{
public:
    struct Desc
    {
        UINT width = 0;
        UINT height = 0;
        UINT bufferCount = 2;
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bool allowTearing = true;     // 지원하는 환경에서만 실제로 켜짐
    };

    void Init(GraphicsDevice& gfx, DescriptorHeap& rtvHeap, HWND hwnd, const Desc& desc);
    void Resize(UINT width, UINT height);     // ⚠ 호출 전에 반드시 gfx.Flush()
    void Present(bool vsync);

    bool IsValid() const { return m_swapChain != nullptr; }

    UINT CurrentIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* CurrentBackBuffer() const { return m_buffers[CurrentIndex()].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const { return m_rtvHeap->Cpu(m_rtvs.index + CurrentIndex()); }

    DXGI_FORMAT Format() const { return m_format; }
    UINT Width() const { return m_width; }
    UINT Height() const { return m_height; }
    UINT BufferCount() const { return m_bufferCount; }
    bool IsTearingSupported() const { return m_tearingSupported; }

private:
    void CreateRtvs();    // Init과 Resize가 같이 사용 → 중복 루프 제거

    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_buffers;

    ID3D12Device* m_device = nullptr;
    DescriptorHeap* m_rtvHeap = nullptr;
    DescriptorHandle m_rtvs;              // bufferCount칸 연속

    UINT m_width = 0, m_height = 0, m_bufferCount = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT m_flags = 0;                     // 생성 때 쓴 플래그 → ResizeBuffers에도 똑같이 넘겨야 함
    bool m_tearingSupported = false;
};