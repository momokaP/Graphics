#pragma once
#include "Renderer/RenderPass.h"

class UIPass : public RenderPass
{
public:
    const wchar_t* Name() const override { return L"UI"; }
    void Init(const RenderInitContext& ctx) override;     // ImGui 컨텍스트 + Win32 백엔드가 먼저 초기화돼 있어야 함
    void Execute(RenderContext& ctx) override;
    void Shutdown() override;
};