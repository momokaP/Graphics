#pragma once
#include <wrl.h>
#include "Renderer/RenderPass.h"
#include "RHI/GpuBuffer.h"
#include "Shaders/ShaderInterop.h"

class SkyboxPass : public RenderPass
{
public:
    const wchar_t* Name() const override { return L"Skybox"; }
    void Init(const RenderInitContext& ctx) override;
    void Execute(RenderContext& ctx) override;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
    GpuBuffer m_vb, m_ib;
    hlsl::ObjectConstants m_object{};
};