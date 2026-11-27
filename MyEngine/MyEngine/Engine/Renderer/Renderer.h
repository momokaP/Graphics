#pragma once
#include <functional>
#include <wrl.h>

#include "RHI/GraphicsDevice.h"
#include "RHI/DescriptorHeap.h"
#include "RHI/SwapChain.h"
#include "RHI/Texture.h"
#include "RHI/LinearUploadAllocator.h"
#include "RHI/DeferredReleaseQueue.h"
#include "RHI/ShaderCompiler.h"

#include "Renderer/RenderContext.h"
#include "Renderer/Passes/SkyboxPass.h"
#include "Renderer/Passes/ForwardPass.h"
#include "Renderer/Passes/WireframePass.h"
#include "Renderer/Passes/BloomPass.h"
#include "Renderer/Passes/TonemapPass.h"
#include "Renderer/Passes/UIPass.h"
#include "Renderer/Passes/IdPass.h"

struct RenderScene;
namespace DirectX { class ResourceUploadBatch; }

class Renderer
{
public:
    static constexpr UINT kFrameCount = 2;

    struct Desc
    {
        HWND hwnd = nullptr;
        UINT width = 0;
        UINT height = 0;
        bool useWarp = false;
    };

    Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // ── 생명주기 ──
    void Init(const Desc& desc);
    void InitUI();                              // ImGui 컨텍스트 + Win32 백엔드 초기화 뒤에 호출
    void Shutdown();                            // GPU 대기 → UI → 리소스 정리

    void Resize(UINT width, UINT height);
    void Render(const RenderScene& scene);      // 기록 → 제출 → Present → 다음 프레임

    // ── 에셋 로딩용 (3단계 AssetManager 전까지 앱에 노출) ──
    void UploadResources(const std::function<void(DirectX::ResourceUploadBatch&)>& record);
    ID3D12Device* Device() const { return m_gfx.Device(); }
    DescriptorHeap& SrvHeap() { return m_srvHeap; }
    void DeferredRelease(Microsoft::WRL::ComPtr<ID3D12Pageable> object);   // GPU가 다 쓴 뒤 해제

    // ── 상태 / 통계 (GUI 표시용) ──
    bool   IsInitialized() const { return m_initialized; }
    bool   VSync() const { return m_vsync; }
    void   SetVSync(bool enable) { m_vsync = enable; }
    bool   IsTearingSupported() const { return m_swapChain.IsTearingSupported(); }
    float  LastGpuWaitMs() const { return m_lastGpuWaitMs; }
    UINT64 UploadBytesUsed() const { return m_uploadAlloc.UsedBytes(); }

    // 피킹 결과 (요청 후 약 3프레임 뒤). 0 = 아무것도 없음, 그 외 = 엔티티 인덱스 + 1
    bool ConsumePickResult(uint32_t& outId);

private:
    void CreateRootSignature();
    void CreateSizeDependentTargets(UINT width, UINT height);
    RenderInitContext MakeInitContext(DirectX::ResourceUploadBatch* upload = nullptr);
    void RecordFrame(const RenderScene& scene);   // ← MyEngine::PopulateCommandList
    void MoveToNextFrame();
    void WaitForGpuIdle();

    void ResolvePick(); // 이전에 기록한 복사 결과 읽기
    void RecordPickCopy(ID3D12GraphicsCommandList* cmd, UINT x, UINT y);

    struct FrameContext
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        UINT64 fenceValue = 0;
    };

    // ⚠ 선언 순서 = 생성 순서, 소멸은 역순 → 디바이스를 맨 앞에 두어 가장 마지막에 정리
    GraphicsDevice m_gfx;
    DescriptorHeap m_rtvHeap, m_dsvHeap, m_srvHeap;
    SwapChain      m_swapChain;
    FrameContext   m_frames[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    LinearUploadAllocator m_uploadAlloc;
    DeferredReleaseQueue  m_releaseQueue;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    ShaderCompiler m_shaderCompiler;

    // 여러 패스가 공유하는 렌더 타깃
    Texture m_sceneColor, m_depth;
    Texture m_idTarget; // R32_UINT, 엔티티 번호
    UINT    m_width = 0, m_height = 0;

    // 피킹 리드백: 프레임 슬롯마다 512바이트 (CPU가 읽을 수 있는 READBACK 힙)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pickReadback;
    bool m_pickPending[kFrameCount] = {}; // 이 슬롯에 복사를 기록했나
    bool m_hasPickResult = false;
    uint32_t m_pickResult = 0;

    // 패스 (실행 순서대로 선언)
    SkyboxPass  m_skyboxPass;
    ForwardPass m_forwardPass;
    WireframePass m_wireframePass;
    BloomPass   m_bloomPass;
    TonemapPass m_tonemapPass;
    UIPass      m_uiPass;
    IdPass      m_idPass;

    UINT  m_frameIndex = 0;
    bool  m_vsync = true;
    bool  m_initialized = false;
    bool  m_uiInitialized = false;
    float m_lastGpuWaitMs = 0.0f;

    float m_dbgPresentMs = 0;
};