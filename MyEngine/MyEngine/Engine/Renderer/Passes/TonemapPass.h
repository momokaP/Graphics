#pragma once
#include <wrl.h>
#include "Renderer/RenderPass.h"

class TonemapPass : public RenderPass
{
public:
    const wchar_t* Name() const override { return L"Tonemap"; }
    void Init(const RenderInitContext& ctx) override;
    void Execute(RenderContext& ctx) override;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
};