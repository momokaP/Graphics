#include "stdafx.h"
#include "RHI/GraphicsDevice.h"
#include "DXSampleHelper.h"   // ThrowIfFailed

#define ENABLE_GBV 0   // [2-A 임시] GPU-based validation

using Microsoft::WRL::ComPtr;

static bool IsUsableAdapter(IDXGIAdapter1* adapter)
{
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) return false;
    return SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
}

static ComPtr<IDXGIAdapter1> FindHardwareAdapter(IDXGIFactory4* factory)
{
    ComPtr<IDXGIAdapter1> adapter;

    // 1순위: 고성능 GPU 우선 (노트북에서 내장 GPU가 잡히는 문제 방지)
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (UINT i = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); ++i)
            if (IsUsableAdapter(adapter.Get())) return adapter;
    }

    // 2순위: 열거 순서대로
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i)
        if (IsUsableAdapter(adapter.Get())) return adapter;

    return nullptr;
}

void GraphicsDevice::Init(bool useWarp)
{
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

#if defined(_DEBUG) && ENABLE_GBV
    ComPtr<ID3D12Debug1> debug1;
    if (SUCCEEDED(debug.As(&debug1)))
        debug1->SetEnableGPUBasedValidation(TRUE);
#endif

    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)));

    if (useWarp)
    {
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> adapter = FindHardwareAdapter(m_factory.Get());
        if (!adapter) throw std::runtime_error("No D3D12 capable GPU found");
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }

    QueryCaps();

#if defined(_DEBUG)
    // 디버그 레이어 에러가 나면 그 줄에서 바로 멈춤 → 원인 찾기가 훨씬 쉬워짐
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_device.As(&infoQueue)))
    {
        // 디버거가 붙어 있을 때만 멈춤. PIX나 exe 단독 실행에서는 로그만 남김
        const BOOL brk = IsDebuggerPresent();
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, brk);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, brk);
    }
#endif

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue)));
    m_queue->SetName(L"Direct Queue");

    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
}

UINT64 GraphicsDevice::Signal()
{
    const UINT64 value = m_nextFenceValue++;
    ThrowIfFailed(m_queue->Signal(m_fence.Get(), value));
    return value;
}

void GraphicsDevice::WaitForFence(UINT64 value)
{
    if (m_fence->GetCompletedValue() >= value) return;
    ThrowIfFailed(m_fence->SetEventOnCompletion(value, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
}

void GraphicsDevice::QueryCaps()
{
    // 셰이더 모델: "내가 아는 최고 버전"부터 물어보고, 런타임이 모르는 값이면(E_INVALIDARG) 한 단계씩 낮춤
    D3D12_FEATURE_DATA_SHADER_MODEL sm = { D3D_HIGHEST_SHADER_MODEL };
    while (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm)))
        && sm.HighestShaderModel > D3D_SHADER_MODEL_5_1)
    {
        sm.HighestShaderModel = D3D_SHADER_MODEL(sm.HighestShaderModel - 1);
    }
    m_caps.highestShaderModel = sm.HighestShaderModel;

    D3D12_FEATURE_DATA_D3D12_OPTIONS12 o12 = {};
    if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &o12, sizeof(o12))))
        m_caps.enhancedBarriers = o12.EnhancedBarriersSupported;

    char buf[128];
    sprintf_s(buf, "[Device] Shader Model %d.%d, Enhanced Barriers %s\n",
        (m_caps.highestShaderModel >> 4) & 0xF, m_caps.highestShaderModel & 0xF,
        m_caps.enhancedBarriers ? "yes" : "no");
    OutputDebugStringA(buf);
}

void GraphicsDevice::Shutdown()
{
    if (m_queue) Flush();
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
}