#pragma once
#include <wrl.h>
#include "Renderer/RenderContext.h"

class RenderPass
{
public:
    RenderPass() = default;
    virtual ~RenderPass() = default;
    RenderPass(const RenderPass&) = delete;              // PSO, 리소스를 소유하므로 복사 금지
    RenderPass& operator=(const RenderPass&) = delete;

    virtual const wchar_t* Name() const = 0;             // PIX 마커 이름
    virtual void Init(const RenderInitContext& ctx) = 0; // PSO, 크기와 무관한 리소스
    virtual bool CreatePipelines(const RenderInitContext& ctx) = 0; // PSO (시작 + 리로드마다)
    virtual void Resize(const RenderInitContext& ctx, UINT width, UINT height) {}   // 크기 의존 리소스
    virtual void Execute(RenderContext& ctx) = 0;        // 커맨드 기록
    virtual void Shutdown() {}

protected:
    // 새 PSO로 교체하고 이전 PSO는 GPU가 다 쓴 뒤 해제
    static void ReplacePso(const RenderInitContext& ctx,
        Microsoft::WRL::ComPtr<ID3D12PipelineState>& slot,
        Microsoft::WRL::ComPtr<ID3D12PipelineState> fresh)
    {
        if (slot)ctx.deferredRelease(slot); // 처음(시작 시)엔 slot이 비어 있음
        slot = std::move(fresh);
    }
};