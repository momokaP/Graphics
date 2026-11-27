#pragma once
#include <wrl.h>
#include "Renderer/RenderPass.h"

class WireframePass : public RenderPass
{
public:
    const wchar_t* Name() const override { return L"WireframePass"; }
    void Init(const RenderInitContext& ctx) override;
    bool CreatePipelines(const RenderInitContext& ctx) override;
    void Execute(RenderContext& ctx) override;
    void Shutdown() override;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
};