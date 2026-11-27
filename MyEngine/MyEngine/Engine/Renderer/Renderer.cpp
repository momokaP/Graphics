#include "stdafx.h"
#include "Paths.h"
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

    // 피킹 리드백 슬롯 위치. GPU 복사(쓰기)와 CPU Map(읽기)이 반드시 같은 값을 써야 함
    //   텍스처 → 버퍼 복사의 시작 오프셋은 512의 배수여야 함 (PLACEMENT_ALIGNMENT)
    constexpr UINT64 kPickSlotSize = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    constexpr UINT64 PickSlotOffset(UINT frameIndex) { return UINT64(frameIndex) * kPickSlotSize; }
}

// ════════════════════════════ 생명주기 ════════════════════════════

void Renderer::Init(const Desc& desc)
{
    m_width = desc.width;
    m_height = desc.height;

    m_gfx.Init(desc.useWarp);

    // 셰이더 모델 확인: 지원 안 하면 PSO 생성의 알 수 없는 실패 대신 여기서 명확하게 멈춤
    if (m_gfx.GetCaps().highestShaderModel < kRequiredShaderModel)
        throw std::runtime_error("This GPU/dirver does not support Shader Model 6.6. Update the GPU driver.");

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

    // 피킹 리드백 버퍼: CPU가 Map해서 읽을 수 있는 힙. 슬롯 오프셋은 512 정렬 (텍스처 복사 규칙)
    {
        auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(kFrameCount * kPickSlotSize);
        ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, // READBACK 힙은 COPY_DEST로
            nullptr, IID_PPV_ARGS(&m_pickReadback)));
        m_pickReadback->SetName(L"Pick Readback");
    }

    m_shaderCompiler.Init(Paths::ShaderSourceDir());

    CreateRootSignature();

    // 패스 초기화 (스카이박스 메시처럼 업로드가 필요한 리소스 포함)
    UploadResources([&](DirectX::ResourceUploadBatch& upload)
        {
            RenderInitContext ctx = MakeInitContext(&upload);
            m_skyboxPass.Init(ctx);
            m_forwardPass.Init(ctx);
            m_wireframePass.Init(ctx);
            m_idPass.Init(ctx);
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
    ResolvePick(); // 이 슬롯에 2프레임 전 기록한 복사가 있으면 읽음
    RecordFrame(scene);

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_gfx.Queue()->ExecuteCommandLists(_countof(lists), lists);

    // m_swapChain.Present(m_vsync);

    // Render() 안, Present 주변 (임시) — Renderer.h에 float m_dbgPresentMs = 0; 추가
    const auto p0 = std::chrono::high_resolution_clock::now();
    m_swapChain.Present(m_vsync);
    const auto p1 = std::chrono::high_resolution_clock::now();
    m_dbgPresentMs = std::chrono::duration<float, std::milli>(p1 - p0).count();

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

    // [임시] 타임라인에서 프레임 구분용. 이 프레임이 받을 fence 값 = 현재 마지막 Signal + 1
    wchar_t frameName[32];
    swprintf_s(frameName, L"Frame %llu", m_gfx.NextFenceValue());
    {
        GpuMarker frameMarker(cmd, frameName);

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

            if (scene.showWireFrame) run(m_wireframePass);

            if (scene.pickRequested && scene.pickX < m_width && scene.pickY < m_height)
            {
                GpuMarker marker(cmd, L"Pick");
                const D3D12_RECT pixel = { LONG(scene.pickX), LONG(scene.pickY),
                                          LONG(scene.pickX) + 1, LONG(scene.pickY) + 1 };

                m_idTarget.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
                cmd->OMSetRenderTargets(1, &m_idTarget.rtv.cpu, FALSE, &m_depth.dsv.cpu); // 씬 깊이 재사용
                cmd->ClearRenderTargetView(m_idTarget.rtv.cpu, m_idTarget.ClearColor(), 1, &pixel); // 1픽셀만
                cmd->ClearDepthStencilView(m_depth.dsv.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &pixel);
                cmd->RSSetScissorRects(1, &pixel); // 클릭한 1픽셀만 래스터화

                // ⚠ Pick은 씬 깊이의 1픽셀을 덮어씀 → 깊이를 읽는 패스보다 뒤에 둘 것
                run(m_idPass);

                cmd->RSSetScissorRects(1, &scene.scissor); // 원래대로
                RecordPickCopy(cmd, scene.pickX, scene.pickY);
            }

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
    }
    ThrowIfFailed(cmd->Close());
}

void Renderer::MoveToNextFrame()
{
    m_frames[m_frameIndex].fenceValue = m_gfx.Signal();
    m_frameIndex = (m_frameIndex + 1) % kFrameCount;

    // [임시 디버그] 실제로 대기가 일어나는 프레임에서만 멈춤
    //const UINT64 dbgWaitFor = m_frames[m_frameIndex].fenceValue;
    //const UINT64 dbgCompleted = m_gfx.CompletedValue();
    //if (dbgCompleted < dbgWaitFor)
    //    __debugbreak();   // 브레이크포인트 없이도 디버거가 여기서 멈춤

    // MoveToNextFrame 안, WaitForFence 바로 위 (임시)
    const UINT64 dbgWaitFor = m_frames[m_frameIndex].fenceValue;
    const UINT64 dbgCompleted = m_gfx.CompletedValue();
    static int s_frames = 0, s_waits = 0;
    ++s_frames;
    if (dbgCompleted < dbgWaitFor) ++s_waits;

    const auto t0 = std::chrono::high_resolution_clock::now();
    m_gfx.WaitForFence(m_frames[m_frameIndex].fenceValue);
    const auto t1 = std::chrono::high_resolution_clock::now();
    m_lastGpuWaitMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // MoveToNextFrame 끝부분, m_lastGpuWaitMs 계산 뒤 (임시)
    /*if (s_frames % 300 == 0)
    {
        char buf[160];
        sprintf_s(buf, "[Sync] VSync=%d  fence waits %d/300  lastFenceWait %.3f ms  lastPresent %.3f ms\n",
            m_vsync, s_waits, m_lastGpuWaitMs, m_dbgPresentMs);
        OutputDebugStringA(buf);
        s_waits = 0;
    }*/

    m_releaseQueue.Collect(m_gfx.CompletedValue());
}

void Renderer::WaitForGpuIdle()
{
    m_gfx.Flush();
}

void Renderer::ResolvePick()
{
    if (!m_pickPending[m_frameIndex]) return;

    // 이 슬롯을 쓴 프레임의 fence는 MoveToNextFrame에서 이미 기다렸음 → 복사 완료 보장
    assert(m_gfx.IsFenceComplete(m_frames[m_frameIndex].fenceValue));
    
    const SIZE_T offset = SIZE_T(PickSlotOffset(m_frameIndex));
    const D3D12_RANGE readRange = { offset, offset + sizeof(uint32_t) }; // CPU가 읽을 범위 
    void* mapped = nullptr;
    ThrowIfFailed(m_pickReadback->Map(0, &readRange, &mapped)); // 반환 포인터는 버퍼 시작
    memcpy(&m_pickResult, static_cast<const uint8_t*>(mapped) + offset, sizeof(uint32_t));
    const D3D12_RANGE noWrite = { 0,0 };
    m_pickReadback->Unmap(0, &noWrite);

    m_hasPickResult = true;
    m_pickPending[m_frameIndex] = false;
}

void Renderer::RecordPickCopy(ID3D12GraphicsCommandList* cmd, UINT x, UINT y)
{
    m_idTarget.Transition(cmd, D3D12_RESOURCE_STATE_COPY_SOURCE);

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_idTarget.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;

    // 버퍼 쪽은 "이 버퍼를 이런 모양의 텍스처로 본다"는 footprint로 기술
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_pickReadback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Offset = PickSlotOffset(m_frameIndex);
    dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_UINT;
    dst.PlacedFootprint.Footprint.Width = 1;
    dst.PlacedFootprint.Footprint.Height = 1;
    dst.PlacedFootprint.Footprint.Depth = 1;
    dst.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

    const D3D12_BOX box = { x,y,0,x + 1,y + 1,1 }; // 원본에서 복사할 영역
    cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

    m_idTarget.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); // 평소 상태로
    m_pickPending[m_frameIndex] = true;
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
    c.shaders = &m_shaderCompiler;
    return c;
}

void Renderer::CreateSizeDependentTargets(UINT width, UINT height)
{
    RenderInitContext ctx = MakeInitContext();
    m_depth.CreateDepth(ctx.device, m_dsvHeap, width, height, kDepthFormat, L"SceneDepth");
    m_sceneColor.CreateRenderTarget(ctx.device, m_rtvHeap, m_srvHeap,
        width, height, kHdrFormat, kSceneClear, L"SceneColor");
    m_bloomPass.Resize(ctx, width, height);
    
    const float kIdClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_idTarget.CreateRenderTarget(ctx.device, m_rtvHeap, m_srvHeap,
        width, height, DXGI_FORMAT_R32_UINT, kIdClear, L"EntityId");
}

bool Renderer::ConsumePickResult(uint32_t& outId)
{
    if (!m_hasPickResult) return false;
    outId = m_pickResult;
    m_hasPickResult = false;
    return true;
}

void Renderer::CreateRootSignature()
{
    // MyEngine::LoadAssets 맨 앞 블록을 그대로 옮김
    using namespace StaticSamplers;
    constexpr auto PS = D3D12_SHADER_VISIBILITY_PIXEL;
    constexpr auto SRV = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    constexpr auto VOLATILE = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    constexpr auto STATIC = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

    RootSignatureBuilder rs;
    rs.CBV(0, D3D12_SHADER_VISIBILITY_ALL)            // RP_Frame
        .CBV(1, D3D12_SHADER_VISIBILITY_VERTEX)         // RP_Object
        .CBV(2, PS)                                     // RP_Material
        .Constants(kPostConstantCount, 3, PS)           // RP_PassConstants
        .Constants(kDebugConstantCount, 4, PS)          // RP_Debug
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