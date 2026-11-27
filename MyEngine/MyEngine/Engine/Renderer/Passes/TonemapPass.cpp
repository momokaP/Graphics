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
#include "Engine/RHI/ShaderCompiler.h"

void TonemapPass::Init(const RenderInitContext& ctx)
{
    
}

bool TonemapPass::CreatePipelines(const RenderInitContext& ctx)
{
    ShaderResult vs = ctx.shaders->Compile({ L"FullscreenVS.hlsl", ShaderTarget::VS });
    ShaderResult ps = ctx.shaders->Compile({ L"BloomCombinePS.hlsl",  ShaderTarget::PS });
    if (!vs.ok || !ps.ok)
        return false;

    auto pso = PipelineBuilder(ctx.rootSignature)
        .VS(vs.bytecode).PS(ps.bytecode)
        .RenderTarget(ctx.backBufferFormat)
        .DepthFormat(kDepthFormat)                 // 같은 바인딩으로 ImGui가 그려지므로 DSV 포맷을 맞춤
        .Cull(D3D12_CULL_MODE_NONE).DepthTest(false)
        .Build(ctx.device, L"Tonemap PSO");
    if (!pso)
    {
        ctx.shaders->ReportError("Tonemap PSO", "CreateGraphicsPipelineState failed (see D3D12 debuf output)");
        return false;
    }

    ReplacePso(ctx, m_pso, std::move(pso));
    return true;
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