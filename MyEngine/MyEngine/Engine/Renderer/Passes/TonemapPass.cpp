#include "Renderer/Passes/TonemapPass.h"
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

void TonemapPass::Init(const RenderInitContext& ctx)
{
    auto vs = LoadShader(Paths::Shader(L"FullscreenVS.cso"));
    auto ps = LoadShader(Paths::Shader(L"BloomCombinePS.cso"));
    m_pso = PipelineBuilder(ctx.rootSignature)
        .VS(vs).PS(ps)
        .RenderTarget(ctx.backBufferFormat)
        .DepthFormat(kDepthFormat)                 // 같은 바인딩으로 ImGui가 그려지므로 DSV 포맷을 맞춤
        .Cull(D3D12_CULL_MODE_NONE).DepthTest(false)
        .Build(ctx.device, L"Tonemap PSO");
}

void TonemapPass::Execute(RenderContext& ctx)
{
    ID3D12GraphicsCommandList* cmd = ctx.cmd;
    const RenderScene& scene = *ctx.scene;
    const RenderTargets& t = *ctx.targets;

    cmd->RSSetViewports(1, &scene.viewport);
    cmd->RSSetScissorRects(1, &scene.scissor);

    hlsl::PostConstants pc{};
    pc.invSize = DirectX::SimpleMath::Vector2(1.0f / t.width, 1.0f / t.height);
    pc.strength = scene.post.bloomStrength;
    pc.exposure = scene.post.exposure;

    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRoot32BitConstants(RP_PassConstants, kPostConstantCount, &pc, 0);
    cmd->SetGraphicsRootDescriptorTable(RP_Tex0, t.sceneColor->srv.gpu);
    cmd->SetGraphicsRootDescriptorTable(RP_Tex1, t.bloom->srv.gpu);
    cmd->DrawInstanced(3, 1, 0, 0);
}