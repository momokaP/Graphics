#pragma once
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
    virtual void Resize(const RenderInitContext& ctx, UINT width, UINT height) {}   // 크기 의존 리소스
    virtual void Execute(RenderContext& ctx) = 0;        // 커맨드 기록
    virtual void Shutdown() {}
};