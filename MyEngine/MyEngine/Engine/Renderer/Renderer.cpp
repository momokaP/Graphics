#include "stdafx.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/RenderScene.h"
#include "Renderer/GpuMarker.h"
#include "RHI/RootSignatureBuilder.h"
#include "DXSampleHelper.h"
#include <directxtk12/ResourceUploadBatch.h>
#include <chrono>

namespace
{
    const float kSceneClear[4] = { 0.2f, 0.2f, 0.25f, 1.0f };
}

// ════════════════════════════ 생명주기 ════════════════════════════

void Renderer::Init(const Desc& desc)
{
    m_width = desc.width;
    m_height = desc.height;

    m_gfx.Init(desc.useWarp);
    ID3D12Device* device = m_gfx.Device();

    m_rtvHeap.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16, false, L"RTV Heap");
    m_dsvHeap.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 4, false, L"DSV Heap");
    m_srvHeap.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true, L"CBV/SRV Heap");

    SwapChain::Desc sc;
    sc.width = m_width;
    sc.height = m_height;
    sc.bufferCount = kFrameCount;
    sc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_swapChain.Init(m_gfx, m_rtvHeap, desc.hwnd, sc);      // ← Win32Application::GetHwnd() 대신 인자로 받음

    for (UINT i = 0; i < kFrameCount; ++i)
    {
        ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_frames[i].allocator)));
        wchar_t name[32];
        swprintf_s(name, L"CommandAllocator[%u]", i);
        m_frames[i].allocator->SetName(name);
    }

    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_frames[0].allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    m_commandList->SetName(L"Main CommandList");
    ThrowIfFailed(m_commandList->Close());

    m_uploadAlloc.Init(device, kFrameCount, 1 * 1024 * 1024, L"Dynamic Upload");

    CreateRootSignature();

    // 패스 초기화 (스카이박스 메시처럼 업로드가 필요한 리소스 포함)
    UploadResources([&](DirectX::ResourceUploadBatch& upload)
        {
            RenderInitContext ctx = MakeInitContext(&upload);
            m_skyboxPass.Init(ctx);
            m_forwardPass.Init(ctx);
            m_bloomPass.Init(ctx);
            m_tonemapPass.Init(ctx);
        });

    CreateSizeDependentTargets(m_width, m_height);
    m_initialized = true;
}

void Renderer::InitUI()
{
    m_uiPass.Init(MakeInitContext());
    m_uiInitialized = true;
}

void Renderer::Shutdown()
{
    if (!m_initialized) return;

    WaitForGpuIdle();             // GPU가 쓰는 리소스가 없어야 정리 가능
    m_releaseQueue.Flush();

    if (m_uiInitialized)
    {
        m_uiPass.Shutdown();
        m_uiInitialized = false;
    }

    m_gfx.Shutdown();
    m_initialized = false;
}

void Renderer::Resize(UINT width, UINT height)
{
    if (!m_initialized || width == 0 || height == 0) return;

    WaitForGpuIdle();             // 진행 중인 프레임이 모두 끝나야 타깃을 다시 만들 수 있음
    m_swapChain.Resize(width, height);
    m_width = width;
    m_height = height;
    CreateSizeDependentTargets(width, height);
}

// ════════════════════════════ 프레임 ════════════════════════════

void Renderer::Render(const RenderScene& scene)
{
    RecordFrame(scene);

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_gfx.Queue()->ExecuteCommandLists(_countof(lists), lists);

    m_swapChain.Present(m_vsync);
    MoveToNextFrame();
}

void Renderer::RecordFrame(const RenderScene& scene)
{
    // 필수 입력 검증: 빠뜨리면 여기서 바로 멈춤 (GPU 크래시보다 찾기 쉬움)
    assert(scene.envMap && scene.irradianceMap && scene.specularMap && "RenderScene: IBL 텍스처 누락");

    FrameContext& frame = m_frames[m_frameIndex];
    ThrowIfFailed(frame.allocator->Reset());
    ThrowIfFailed(m_commandList->Reset(frame.allocator.Get(), nullptr));
    m_uploadAlloc.BeginFrame(m_frameIndex);

    ID3D12GraphicsCommandList* cmd = m_commandList.Get();

    RenderTargets targets;
    targets.sceneColor = &m_sceneColor;
    targets.depth = &m_depth;
    targets.backBuffer = &m_swapChain.CurrentBackBuffer();
    targets.backBufferRtv = m_swapChain.CurrentRtv();
    targets.width = m_width;
    targets.height = m_height;

    RenderContext ctx{ cmd, &m_uploadAlloc, &scene, &targets };
    auto run = [&](RenderPass& pass)
        {
            GpuMarker marker(cmd, pass.Name());
            pass.Execute(ctx);
        };

    // ════ 프레임 공통 바인딩 (모든 패스의 전제) ════
    cmd->SetGraphicsRootSignature(m_rootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Heap() };
    cmd->SetDescriptorHeaps(_countof(heaps), heaps);
    cmd->SetGraphicsRootConstantBufferView(RP_Frame, m_uploadAlloc.Push(scene.frame));
    cmd->SetGraphicsRootDescriptorTable(RP_EnvMap, scene.envMap->srv.gpu);
    cmd->SetGraphicsRootDescriptorTable(RP_Irradiance, scene.irradianceMap->srv.gpu);
    cmd->SetGraphicsRootDescriptorTable(RP_Specular, scene.specularMap->srv.gpu);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ════ 씬 (HDR) ════
    {
        GpuMarker marker(cmd, L"Scene");
        m_sceneColor.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->OMSetRenderTargets(1, &m_sceneColor.rtv.cpu, FALSE, &m_depth.dsv.cpu);
        cmd->ClearRenderTargetView(m_sceneColor.rtv.cpu, m_sceneColor.ClearColor(), 0, nullptr);
        cmd->ClearDepthStencilView(m_depth.dsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        cmd->RSSetViewports(1, &scene.viewport);
        cmd->RSSetScissorRects(1, &scene.scissor);

        run(m_skyboxPass);
        run(m_forwardPass);

        m_sceneColor.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    // ════ 후처리 ════
    run(m_bloomPass);
    targets.bloom = &m_bloomPass.Output();

    // ════ 백버퍼 (LDR) ════
    targets.backBuffer->Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->OMSetRenderTargets(1, &targets.backBufferRtv, FALSE, &m_depth.dsv.cpu);   // DSV는 ImGui PSO 포맷과 맞추기 위함
    cmd->ClearRenderTargetView(targets.backBufferRtv, kSceneClear, 0, nullptr);

    run(m_tonemapPass);
    if (m_uiInitialized)
        run(m_uiPass);        // ⚠ 항상 마지막 (ImGui가 루트 시그니처를 바꿈)

    targets.backBuffer->Transition(cmd, D3D12_RESOURCE_STATE_PRESENT);
    ThrowIfFailed(cmd->Close());
}

void Renderer::MoveToNextFrame()
{
    m_frames[m_frameIndex].fenceValue = m_gfx.Signal();
    m_frameIndex = (m_frameIndex + 1) % kFrameCount;

    const auto t0 = std::chrono::high_resolution_clock::now();
    m_gfx.WaitForFence(m_frames[m_frameIndex].fenceValue);
    const auto t1 = std::chrono::high_resolution_clock::now();
    m_lastGpuWaitMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    m_releaseQueue.Collect(m_gfx.CompletedValue());
}

void Renderer::WaitForGpuIdle()
{
    m_gfx.Flush();
}

// ════════════════════════════ 에셋 지원 ════════════════════════════

void Renderer::UploadResources(const std::function<void(DirectX::ResourceUploadBatch&)>& record)
{
    DirectX::ResourceUploadBatch upload(m_gfx.Device());
    upload.Begin();
    record(upload);                         // 호출자가 텍스처/버퍼 생성을 기록
    upload.End(m_gfx.Queue()).wait();       // 업로드가 끝날 때까지 대기 → 반환 후 바로 SRV 생성 가능
}

void Renderer::DeferredRelease(Microsoft::WRL::ComPtr<ID3D12Pageable> object)
{
    // 지금 기록 중인 프레임까지 GPU가 끝내면 해제 (다음 Signal 값 기준)
    m_releaseQueue.Push(std::move(object), m_gfx.NextFenceValue());
}

// ════════════════════════════ 내부 ════════════════════════════

RenderInitContext Renderer::MakeInitContext(DirectX::ResourceUploadBatch* upload)
{
    RenderInitContext c;
    c.device = m_gfx.Device();
    c.queue = m_gfx.Queue();
    c.rootSignature = m_rootSignature.Get();
    c.rtvHeap = &m_rtvHeap;
    c.dsvHeap = &m_dsvHeap;
    c.srvHeap = &m_srvHeap;
    c.backBufferFormat = m_swapChain.Format();
    c.frameCount = kFrameCount;
    c.upload = upload;
    return c;
}

void Renderer::CreateSizeDependentTargets(UINT width, UINT height)
{
    RenderInitContext ctx = MakeInitContext();
    m_depth.CreateDepth(ctx.device, m_dsvHeap, width, height, kDepthFormat, L"SceneDepth");
    m_sceneColor.CreateRenderTarget(ctx.device, m_rtvHeap, m_srvHeap,
        width, height, kHdrFormat, kSceneClear, L"SceneColor");
    m_bloomPass.Resize(ctx, width, height);
}

void Renderer::CreateRootSignature()
{
    // MyEngine::LoadAssets 맨 앞 블록을 그대로 옮김
    using namespace StaticSamplers;
    constexpr auto PS = D3D12_SHADER_VISIBILITY_PIXEL;
    constexpr auto SRV = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    constexpr auto VOLATILE = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    constexpr auto STATIC = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;

    RootSignatureBuilder rs;
    rs.CBV(0, D3D12_SHADER_VISIBILITY_ALL)            // RP_Frame
        .CBV(1, D3D12_SHADER_VISIBILITY_VERTEX)         // RP_Object
        .CBV(2, PS)                                     // RP_Material
        .Constants(kPostConstantCount, 3, PS)           // RP_PassConstants
        .Table(SRV, 1, 0, PS, VOLATILE)                 // RP_Tex0
        .Table(SRV, 1, 1, PS, VOLATILE)                 // RP_Tex1
        .Table(SRV, 1, 2, PS, VOLATILE)                 // RP_Tex2
        .Table(SRV, 1, 3, PS, VOLATILE)                 // RP_Tex3
        .Table(SRV, 1, 4, PS, STATIC)                   // RP_EnvMap
        .Table(SRV, 1, 5, PS, STATIC)                   // RP_Irradiance
        .Table(SRV, 1, 6, PS, STATIC)                   // RP_Specular
        .StaticSampler(PointClamp(0))
        .StaticSampler(LinearClamp(1))
        .StaticSampler(LinearWrap(2))
        .StaticSampler(AnisoWrap(3))
        .Flags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
    assert(rs.ParamCount() == RP_Count);
    m_rootSignature = rs.Build(m_gfx.Device(), L"Engine RootSig");
}