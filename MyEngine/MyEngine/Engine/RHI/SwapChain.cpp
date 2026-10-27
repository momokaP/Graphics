#include "stdafx.h"
#include "RHI/SwapChain.h"
#include "RHI/GraphicsDevice.h"
#include "DXSampleHelper.h"
#include <cstdio>

using Microsoft::WRL::ComPtr;

void SwapChain::Init(GraphicsDevice& gfx, DescriptorHeap& rtvHeap, HWND hwnd, const Desc& desc)
{
    m_device = gfx.Device();
    m_rtvHeap = &rtvHeap;
    m_width = desc.width;
    m_height = desc.height;
    m_bufferCount = desc.bufferCount;
    m_format = desc.format;

    // 테어링 지원 확인: VSync를 끈 채로 프레임 상한 없이 Present하려면 필요 (FPS 측정용)
    if (desc.allowTearing)
    {
        ComPtr<IDXGIFactory5> factory5;
        BOOL allow = FALSE;
        if (SUCCEEDED(gfx.Factory()->QueryInterface(IID_PPV_ARGS(&factory5))) &&
            SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
        {
            m_tearingSupported = (allow == TRUE);
        }
    }
    m_flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = m_width;
    sd.Height = m_height;
    sd.Format = m_format;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = m_bufferCount;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;              // flip 모델은 MSAA 백버퍼 불가 (MSAA는 별도 타깃에서)
    sd.Flags = m_flags;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(gfx.Factory()->CreateSwapChainForHwnd(
        gfx.Queue(),                      // D3D12에서는 디바이스가 아니라 '큐'를 넘김
        hwnd, &sd, nullptr, nullptr, &swapChain1));

    // Alt+Enter 전체화면 전환 막기 (전환하려면 직접 구현해야 함)
    ThrowIfFailed(gfx.Factory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain1.As(&m_swapChain));   // GetCurrentBackBufferIndex는 IDXGISwapChain3부터

    m_buffers.resize(m_bufferCount);
    m_rtvs = rtvHeap.Allocate(m_bufferCount);
    CreateRtvs();
}

void SwapChain::CreateRtvs()
{
    for (UINT i = 0; i < m_bufferCount; ++i)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer)));
        m_buffers[i].Attach(buffer, D3D12_RESOURCE_STATE_PRESENT);   // 스왑체인 버퍼는 PRESENT로 시작

        wchar_t name[32];
        swprintf_s(name, L"BackBuffer[%u]", i);
        m_buffers[i].SetName(name);

        m_device->CreateRenderTargetView(m_buffers[i].Get(), nullptr, m_rtvHeap->Cpu(m_rtvs.index + i));
    }
}

void SwapChain::Resize(UINT width, UINT height)
{
    if (width == 0 || height == 0) return;

    for (auto& buffer : m_buffers)
        buffer.Reset();                 // 참조가 하나라도 남아 있으면 ResizeBuffers가 실패

    ThrowIfFailed(m_swapChain->ResizeBuffers(m_bufferCount, width, height, m_format, m_flags));
    m_width = width;
    m_height = height;

    CreateRtvs();                         // 같은 RTV 칸에 다시 생성 (디스크립터 재할당 불필요)
}

void SwapChain::Present(bool vsync)
{
    const UINT syncInterval = vsync ? 1 : 0;
    const UINT presentFlags = (!vsync && m_tearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);

    // GPU 행(hang), 드라이버 리셋 등. 원인 코드를 남겨야 디버깅이 가능
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        char msg[64];
        sprintf_s(msg, "Device removed. Reason: 0x%08X\n",
            static_cast<unsigned>(m_device->GetDeviceRemovedReason()));
        OutputDebugStringA(msg);
    }
    ThrowIfFailed(hr);
}