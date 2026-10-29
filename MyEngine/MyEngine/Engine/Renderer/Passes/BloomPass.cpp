#include "Renderer/Passes/BloomPass.h"
#include <algorithm>
#include "stdafx.h"
#include "Renderer/RenderTypes.h"
#include "Renderer/RenderScene.h"
#include "RHI/PipelineBuilder.h"
#include "RHI/LinearUploadAllocator.h"
#include "RHI/Texture.h"
#include "RHI/GpuBuffer.h"
#include "DXSampleHelper.h"     // LoadShader, ThrowIfFailed
#include "Paths.h"
#include "Vertex.h"

void BloomPass::Init(const RenderInitContext& ctx)
{
    auto vs = LoadShader(Paths::Shader(L"FullscreenVS.cso"));
    auto make = [&](const wchar_t* psFile, const wchar_t* name)
        {
            auto ps = LoadShader(Paths::Shader(psFile));
            return PipelineBuilder(ctx.rootSignature)
                .VS(vs).PS(ps)
                .RenderTarget(kHdrFormat)                     // 깊이 없음 → DepthFormat 생략(UNKNOWN)
                .Cull(D3D12_CULL_MODE_NONE).DepthTest(false)
                .Build(ctx.device, name);
        };
    m_brightPSO = make(L"BloomBrightPS.cso", L"Bloom Bright PSO");
    m_blurPSO = make(L"BloomBlurPS.cso", L"Bloom Blur PSO");
}

void BloomPass::Resize(const RenderInitContext& ctx, UINT width, UINT height)
{
    static const float kBlack[4] = { 0, 0, 0, 1 };
    m_width = (std::max)(1u, width / 2);       // 절반 해상도
    m_height = (std::max)(1u, height / 2);
    m_bloom[0].CreateRenderTarget(ctx.device, *ctx.rtvHeap, *ctx.srvHeap,
        m_width, m_height, kHdrFormat, kBlack, L"Bloom[0]");
    m_bloom[1].CreateRenderTarget(ctx.device, *ctx.rtvHeap, *ctx.srvHeap,
        m_width, m_height, kHdrFormat, kBlack, L"Bloom[1]");
}

void BloomPass::Execute(RenderContext& ctx)
{
    ID3D12GraphicsCommandList* cmd = ctx.cmd;
    const PostSettings& s = ctx.scene->post;

    CD3DX12_VIEWPORT vp(0.0f, 0.0f, float(m_width), float(m_height));
    CD3DX12_RECT     rect(0, 0, LONG(m_width), LONG(m_height));
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &rect);

    hlsl::PostConstants pc{};
    pc.invSize = DirectX::SimpleMath::Vector2(1.0f / m_width, 1.0f / m_height);
    pc.threshold = s.bloomThreshold;

    // 전용 텍스처의 상태 전환은 패스가 직접 (외부에 노출되지 않는 리소스)
    auto draw = [&](ID3D12PipelineState* pso, Texture& target, const Texture& src)
        {
            target.Transition(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
            cmd->OMSetRenderTargets(1, &target.rtv.cpu, FALSE, nullptr);
            cmd->SetPipelineState(pso);
            cmd->SetGraphicsRoot32BitConstants(RP_PassConstants, kPostConstantCount, &pc, 0);
            cmd->SetGraphicsRootDescriptorTable(RP_Tex0, src.srv.gpu);
            cmd->DrawInstanced(3, 1, 0, 0);
            target.Transition(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        };

    draw(m_brightPSO.Get(), m_bloom[0], *ctx.targets->sceneColor);   // 입력: SceneColor (이미 SRV 상태)
    for (int i = 0; i < s.bloomBlurCount; ++i)
    {
        pc.direction = DirectX::SimpleMath::Vector2(1.0f, 0.0f);
        draw(m_blurPSO.Get(), m_bloom[1], m_bloom[0]);
        pc.direction = DirectX::SimpleMath::Vector2(0.0f, 1.0f);
        draw(m_blurPSO.Get(), m_bloom[0], m_bloom[1]);
    }
}